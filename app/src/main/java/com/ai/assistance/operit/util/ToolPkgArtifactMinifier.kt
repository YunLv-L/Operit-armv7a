package com.ai.assistance.operit.util

import android.content.Context
import com.ai.assistance.operit.core.tools.packTool.ToolPkgArchiveParser
import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.charset.StandardCharsets
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

/** Produces standard ToolPkg and script artifacts with executable JavaScript AST-minified. */
object ToolPkgArtifactMinifier {
    fun minifyArtifactFile(context: Context, sourceFile: File, isToolPkg: Boolean): ByteArray {
        return ToolPkgJsAstMinifier(context).use { minifier ->
            if (isToolPkg) {
                minifyToolPkgArchive(sourceFile, minifier)
            } else {
                minifyScriptFile(sourceFile, minifier)
            }
        }
    }

    private fun minifyScriptFile(sourceFile: File, minifier: ToolPkgJsAstMinifier): ByteArray {
        return minifyJavaScriptBytes(sourceFile.readBytes(), sourceFile.name, minifier)
    }

    private fun minifyToolPkgArchive(sourceFile: File, minifier: ToolPkgJsAstMinifier): ByteArray {
        val manifestPreview =
            ToolPkgArchiveParser.readToolPkgManifestPreview { sourceFile.inputStream() }
                ?: throw IllegalArgumentException("manifest.hjson or manifest.json not found")
        val manifestBasePath = manifestPreview.entryName.substringBeforeLast('/', missingDelimiterValue = "")
        val manifestEntryName =
            ToolPkgArchiveParser.normalizeZipEntryPath(manifestPreview.entryName)
                ?: throw IllegalArgumentException("Invalid toolpkg manifest entry name")
        val executableEntryNames = linkedSetOf<String>()
        val resourceEntryRoots = linkedSetOf<String>()

        ToolPkgArchiveParser.resolveManifestRelativeZipEntryPath(
            manifestBasePath,
            manifestPreview.manifest.main
        )?.let(executableEntryNames::add)
        manifestPreview.manifest.subpackages.forEach { subpackage ->
            ToolPkgArchiveParser.resolveManifestRelativeZipEntryPath(manifestBasePath, subpackage.entry)
                ?.let(executableEntryNames::add)
        }
        manifestPreview.manifest.resources.forEach { resource ->
            ToolPkgArchiveParser.resolveManifestRelativeResourcePath(manifestBasePath, resource.path)
                ?.let(resourceEntryRoots::add)
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
                        val originalBytes = archive.getInputStream(entry).use { input -> input.readBytes() }
                        val outputEntryBytes =
                            if (
                                normalizedName != null &&
                                    normalizedName != manifestEntryName &&
                                    shouldMinifyToolPkgEntry(
                                        normalizedName,
                                        executableEntryNames,
                                        resourceEntryRoots
                                    )
                            ) {
                                minifyJavaScriptBytes(originalBytes, normalizedName, minifier)
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

    private fun minifyJavaScriptBytes(
        bytes: ByteArray,
        entryName: String,
        minifier: ToolPkgJsAstMinifier
    ): ByteArray {
        val source = bytes.toString(StandardCharsets.UTF_8)
        val minified = minifyJavaScriptSourcePreservingMetadata(source, entryName, minifier)
        return minified.toByteArray(StandardCharsets.UTF_8)
    }

    private fun minifyJavaScriptSourcePreservingMetadata(
        source: String,
        entryName: String,
        minifier: ToolPkgJsAstMinifier
    ): String {
        val split = splitLeadingMetadataBlock(source)
        if (split != null) {
            val body = split.body.trim()
            require(body.isNotEmpty()) { "JavaScript body after METADATA is empty for $entryName" }
            return split.metadataBlock + minifier.minify(body, entryName)
        }
        return minifier.minify(source, entryName)
    }

    private fun splitLeadingMetadataBlock(source: String): MetadataSplit? {
        val trimmed = source.trimStart()
        val leadingWhitespaceSize = source.length - trimmed.length
        if (!trimmed.startsWith("/*")) return null

        val commentBody = trimmed.substring(2)
        val label = commentBody.trimStart()
        if (!startsWithMetadataLabel(label)) return null

        val commentEnd = trimmed.indexOf("*/")
        if (commentEnd < 0) return null

        val metadataEnd = leadingWhitespaceSize + commentEnd + 2
        return MetadataSplit(
            metadataBlock = source.substring(0, metadataEnd),
            body = source.substring(metadataEnd)
        )
    }

    private fun startsWithMetadataLabel(commentBody: String): Boolean {
        if (!commentBody.startsWith("METADATA")) return false
        val afterLabel = commentBody.substring("METADATA".length)
        val first = afterLabel.firstOrNull() ?: return true
        return first.isWhitespace() || first == '*'
    }

    private fun shouldMinifyToolPkgEntry(
        normalizedName: String,
        executableEntryNames: Set<String>,
        resourceEntryRoots: Set<String>
    ): Boolean {
        if (resourceEntryRoots.any { root -> normalizedName == root || normalizedName.startsWith("$root/") }) {
            return false
        }
        if (executableEntryNames.contains(normalizedName)) return true
        val extension = normalizedName.substringAfterLast('.', missingDelimiterValue = "").lowercase()
        return extension in setOf("js", "mjs", "cjs", "ts", "jsx", "tsx")
    }

    private data class MetadataSplit(
        val metadataBlock: String,
        val body: String
    )
}
