import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "com.ai.assistance.llama"
    compileSdk = 36

    defaultConfig {
        minSdk = 26
        targetSdk = 34

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles("consumer-rules.pro")

        ndk {
            abiFilters.addAll(listOf("armeabi-v7a", "arm64-v8a"))
        }

        externalNativeBuild {
            cmake {
                // 为 armeabi-v7a 添加 Neon 兼容和禁用 LlamaFile 的标志
                cppFlags += listOf(
                    "-std=c++17",
                    "-fno-emulated-tls",
                    "-mfpu=neon-vfpv4",
                    "-DGGML_USE_NEON=1",
                    "-DGGML_USE_ARM_NEON=1",
                    "-D__ARM_NEON=1",
                    "-DGGML_USE_LLAMAFILE=OFF"
                )
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_PLATFORM=android-26",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                    "-DLLAMA_BUILD_COMMON=ON",
                    "-DGGML_USE_NEON=1",
                    "-DGGML_USE_ARM_NEON=1",
                    "-DGGML_USE_LLAMAFILE=OFF"
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

}

kotlin {
    compilerOptions {
        jvmTarget = JvmTarget.JVM_17
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}
