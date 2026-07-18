package com.ai.assistance.operit.util

import com.ai.assistance.operit.core.tools.packTool.ToolPkgArchiveParser
import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.charset.StandardCharsets
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

object ToolPkgProtection {
    const val PROTECTION_ID = "operit-protected-v1"

    private val magic = byteArrayOf(
        'O'.code.toByte(),
        'P'.code.toByte(),
        'T'.code.toByte(),
        'P'.code.toByte(),
        'R'.code.toByte(),
        'O'.code.toByte(),
        'T'.code.toByte(),
        '1'.code.toByte()
    )

    fun isProtected(bytes: ByteArray): Boolean {
        if (bytes.size < magic.size) return false
        for (index in magic.indices) {
            if (bytes[index] != magic[index]) return false
        }
        return true
    }

    fun decryptIfNeeded(bytes: ByteArray): ByteArray {
        return if (isProtected(bytes)) ToolPkgProtectionNative.decrypt(bytes) else bytes
    }

    fun decodeUtf8(bytes: ByteArray): String {
        return decryptIfNeeded(bytes).toString(StandardCharsets.UTF_8)
    }

    fun encrypt(bytes: ByteArray): ByteArray {
        require(bytes.isNotEmpty()) { "Cannot protect empty content" }
        return if (isProtected(bytes)) bytes else ToolPkgProtectionNative.encrypt(bytes)
    }

    fun protectArtifactFile(sourceFile: File, isToolPkg: Boolean): ByteArray {
        require(ToolPkgProtectionNative.isSecretConfigured()) {
            "ToolPkg protection secret is not configured for this build"
        }
        return if (isToolPkg) protectToolPkgArchive(sourceFile) else encrypt(sourceFile.readBytes())
    }

    private fun protectToolPkgArchive(sourceFile: File): ByteArray {
        val manifestPreview =
            ToolPkgArchiveParser.readToolPkgManifestPreview { sourceFile.inputStream() }
                ?: throw IllegalArgumentException("manifest.hjson or manifest.json not found")
        val manifest = manifestPreview.manifest
        val manifestBasePath = manifestPreview.entryName.substringBeforeLast('/', missingDelimiterValue = "")
        val protectedEntryNames = linkedSetOf<String>()
        val plainResourceEntryRoots = linkedSetOf<String>()

        ToolPkgArchiveParser.resolveManifestRelativeZipEntryPath(manifestBasePath, manifest.main)
            ?.let(protectedEntryNames::add)
        manifest.subpackages.forEach { subpackage ->
            ToolPkgArchiveParser.resolveManifestRelativeZipEntryPath(manifestBasePath, subpackage.entry)
                ?.let(protectedEntryNames::add)
        }
        manifest.resources.forEach { resource ->
            ToolPkgArchiveParser.resolveManifestRelativeResourcePath(manifestBasePath, resource.path)
                ?.let(plainResourceEntryRoots::add)
        }

        val outputBytes = ByteArrayOutputStream()
        ZipFile(sourceFile).use { archive ->
            ZipOutputStream(outputBytes).use { zipOutput ->
                val entries = archive.entries()
                while (entries.hasMoreElements()) {
                    val entry = entries.nextElement()
                    val copiedEntry = ZipEntry(entry.name).apply {
                        time = entry.time
                        comment = entry.comment
                    }
                    zipOutput.putNextEntry(copiedEntry)
                    if (!entry.isDirectory) {
                        val normalizedName = ToolPkgArchiveParser.normalizeZipEntryPath(entry.name)
                        val originalBytes = archive.getInputStream(entry).use { it.readBytes() }
                        val outputEntryBytes =
                            if (
                                shouldProtectToolPkgEntry(
                                    normalizedName = normalizedName,
                                    protectedEntryNames = protectedEntryNames,
                                    plainResourceEntryRoots = plainResourceEntryRoots
                                )
                            ) {
                                encrypt(originalBytes)
                            } else {
                                originalBytes
                            }
                        zipOutput.write(outputEntryBytes)
                    }
                    zipOutput.closeEntry()
                }
            }
        }
        return outputBytes.toByteArray()
    }

    private fun shouldProtectToolPkgEntry(
        normalizedName: String?,
        protectedEntryNames: Set<String>,
        plainResourceEntryRoots: Set<String>
    ): Boolean {
        if (normalizedName.isNullOrBlank()) return false
        if (isPlainResourceEntry(normalizedName, plainResourceEntryRoots)) return false
        if (protectedEntryNames.contains(normalizedName)) return true
        val extension = normalizedName.substringAfterLast('.', missingDelimiterValue = "").lowercase()
        return extension in setOf("js", "mjs", "cjs", "ts", "jsx", "tsx")
    }

    private fun isPlainResourceEntry(
        normalizedName: String,
        plainResourceEntryRoots: Set<String>
    ): Boolean {
        return plainResourceEntryRoots.any { resourceRoot ->
            normalizedName == resourceRoot || normalizedName.startsWith("$resourceRoot/")
        }
    }
}

internal object ToolPkgProtectionNative {
    init {
        System.loadLibrary("streamnative")
    }

    external fun isSecretConfigured(): Boolean

    external fun encrypt(bytes: ByteArray): ByteArray

    external fun decrypt(bytes: ByteArray): ByteArray
}
