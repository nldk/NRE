plugins {
    `java-library`
}

java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(25)
    }
}

dependencies {
    testImplementation(platform("org.junit:junit-bom:5.14.5"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.withType<JavaCompile>().configureEach {
    options.compilerArgs.add("-Xlint:all")
}

tasks.test {
    useJUnitPlatform()
    jvmArgs("--enable-native-access=ALL-UNNAMED")
    // The tests load libnre_c from NRE_LIB_DIR (if set) and check the enums against the header.
    val libDir = providers.environmentVariable("NRE_LIB_DIR").orElse("")
    inputs.property("nreLibDir", libDir)
    libDir.get().takeIf { it.isNotEmpty() }?.let { environment("NRE_LIB_DIR", it) }
    val header = rootDir.resolve("../../include/nre.h").canonicalFile
    inputs.file(header)
    systemProperty("nre.header", header.path)
    testLogging {
        events("passed", "skipped", "failed")
        showStandardStreams = true
        exceptionFormat = org.gradle.api.tasks.testing.logging.TestExceptionFormat.FULL
    }
}
