#!/bin/sh
# Builds dist/Convallaria.apk (arm64, 32-bit arm, x86_64 for emulators/Chromebooks) without Gradle. Run from the repo root.
# Needs the Android SDK at ~/Android/Sdk (a platform, build-tools 34.0.0, NDK 27) and a JDK.
# Signs with ~/.android/convallaria.keystore, made on first run: keep it, phones only accept
# updates signed with the same key.
set -e
SDK=${ANDROID_HOME:-$HOME/Android/Sdk}
BT=$SDK/build-tools/34.0.0
NDK=$SDK/ndk/27.2.12479018
JAR=$(ls -d "$SDK"/platforms/android-* | sort -V | tail -1)/android.jar
OUT=build-android
for abi in arm64-v8a armeabi-v7a x86_64; do
  cmake -S . -B $OUT/$abi -G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=$abi -DANDROID_PLATFORM=24 -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build $OUT/$abi
  mkdir -p $OUT/apk/lib/$abi
  "$NDK"/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip -o $OUT/apk/lib/$abi/libmain.so $OUT/$abi/libmain.so
done
"$BT/aapt2" compile --dir android/res -o $OUT/res.zip
"$BT/aapt2" link -o $OUT/unaligned.apk -I "$JAR" \
  --manifest android/AndroidManifest.xml -R $OUT/res.zip -A assets --auto-add-overlay
(cd $OUT/apk && python -c "
import zipfile, pathlib
with zipfile.ZipFile('../unaligned.apk', 'a', zipfile.ZIP_DEFLATED) as z:
    for f in pathlib.Path('lib').rglob('*.so'): z.write(f, f.as_posix())")
"$BT/zipalign" -f -p 4 $OUT/unaligned.apk $OUT/aligned.apk
KS=$HOME/.android/convallaria.keystore
[ -f "$KS" ] || { mkdir -p "$(dirname "$KS")"; keytool -genkeypair -keystore "$KS" -alias convallaria -keyalg RSA -keysize 2048 \
  -validity 10000 -storepass convallaria -keypass convallaria -dname "CN=Convallaria" >/dev/null; }
mkdir -p dist
"$BT/apksigner.bat" sign --ks "$KS" --ks-pass pass:convallaria --out dist/Convallaria.apk $OUT/aligned.apk 2>/dev/null || \
  "$BT/apksigner" sign --ks "$KS" --ks-pass pass:convallaria --out dist/Convallaria.apk $OUT/aligned.apk
echo "built dist/Convallaria.apk"
