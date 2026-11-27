// Shared settings live in each subproject's build file; this root project only groups them.
plugins {
    kotlin("jvm") version "2.4.21" apply false
}

subprojects {
    group = "nre"
    version = "0.3.0"
}
