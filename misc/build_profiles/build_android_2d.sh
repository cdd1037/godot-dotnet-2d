#!/usr/bin/env bash
# Requires .NET SDK 10.0.401, JDK 17, Android SDK 36/build-tools 36.1.0,
# NDK 29.0.14206865, Python + SCons, and Gradle 8.11.1 (or the wrapper).
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${ANDROID_HOME:?Set ANDROID_HOME to the Android SDK}"
: "${JAVA_HOME:?Set JAVA_HOME to JDK 17}"
export DOTNET_PROCESSOR_COUNT="${DOTNET_PROCESSOR_COUNT:-8}"
# Paired templates. Bound LLD as well as SCons: hardware thread count can exceed RAM.
scons profile=misc/build_profiles/android_release_2d.py target=template_debug lto=none -j8
scons profile=misc/build_profiles/android_release_2d.py target=template_release lto=thin \
    'linkflags=-Wl,--threads=4,--thinlto-jobs=4' -j4
(
    cd platform/android/java
    ./gradlew --no-daemon --max-workers=4 \
        '-Dorg.gradle.jvmargs=-Xmx2048m -XX:ActiveProcessorCount=4' generateGodotMonoTemplates
)
printf '%s\n' 'Templates: bin/android_monoDebug.apk, bin/android_monoRelease.apk, bin/android_source.zip'
