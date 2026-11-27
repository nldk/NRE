plugins {
    kotlin("jvm")
}

kotlin {
    jvmToolchain(25)
}

dependencies {
    api(project(":nre"))

    testImplementation(platform("org.junit:junit-bom:5.14.5"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.test {
    useJUnitPlatform()
    jvmArgs("--enable-native-access=ALL-UNNAMED")
    // Headless by default. NRE_WINDOW_TESTS=1 also runs NreWindowTest, which opens a window (needs NRE_LIB_DIR and
    // the example shaders built).
    for (name in listOf("NRE_LIB_DIR", "NRE_WINDOW_TESTS", "NRE_SHADER_DIR")) {
        val value = providers.environmentVariable(name).orElse("")
        inputs.property(name, value)
        value.get().takeIf { it.isNotEmpty() }?.let { environment(name, it) }
    }
    systemProperty("nre.repo", rootDir.resolve("../..").canonicalPath)
    testLogging {
        events("passed", "skipped", "failed")
        showStandardStreams = true
        exceptionFormat = org.gradle.api.tasks.testing.logging.TestExceptionFormat.FULL
    }
}
