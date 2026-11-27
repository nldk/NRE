plugins {
    kotlin("jvm")
}

kotlin {
    jvmToolchain(25)
}

dependencies {
    implementation(project(":nre"))
    implementation(project(":nre-kotlin"))
}

// ./gradlew :examples:runJava   (add --args=scene for SceneMode)
// ./gradlew :examples:runKotlin (add --args=scene for SceneMode)
// ./gradlew :examples:runKotlinDsl (add --args=scene for SceneMode)
fun registerRun(name: String, main: String, what: String) = tasks.register<JavaExec>(name) {
    group = "application"
    description = "Runs the $what orbit example."
    classpath = sourceSets.main.get().runtimeClasspath
    mainClass = main
    javaLauncher = javaToolchains.launcherFor { languageVersion = JavaLanguageVersion.of(25) }
    jvmArgs("--enable-native-access=ALL-UNNAMED")
}

registerRun("runJava", "nre.examples.OrbitExample", "Java")
registerRun("runKotlin", "nre.examples.kt.OrbitExampleKt", "Kotlin")
registerRun("runKotlinDsl", "nre.examples.kt.OrbitDslKt", "Kotlin DSL")
