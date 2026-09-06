# 開発環境と再現手順

更新日: 2026-09-06

## 方針

RoadWeaveの主開発環境はmacOSとする。実機のUSB接続、書き込み、monitorはmacOSで確認し、Windowsでも同じESP-IDF versionとコマンド列を再現できるようにする。どちらの環境でも`firmware/toolchain.lock`をversionのsource of truthとし、ESP-IDFは`v5.4.4`を使う。

macOSでは2026-09-02にHomebrewのCMake `4.4.3`とNinja `1.13.2`でESP32-S3 buildを確認済み。これは再現実績であり、Windowsへ同じhost tool versionを強制するものではない。WindowsではESP-IDF Tools Installerが管理する互換versionを使い、Linux CIでは公式ESP-IDF containerのtoolchainを使う。

WindowsではEspressifのWindows向けESP-IDF環境を使い、PowerShellから`idf.py`を実行する。ESP-IDFのexport済みターミナルで作業を始めること。WSLはLinux系の補助作業には使えるが、USB flashとmonitorの経路には使わない。WindowsのUSBドライバとCOM portを使うnative PowerShell手順を正とする。

公式Windows installerはPython、Git、cross compiler、CMake、Ninjaをまとめて導入する。install pathは空白・括弧を避け、ESP-IDFとtoolsのpathを90文字以内にする。

## macOS（主開発環境）

### install場所とPython version（2026-09-05追記）

- ESP-IDF本体は`~/esp/v5.4.4/esp-idf`、toolsは`~/.espressif`（`IDF_TOOLS_PATH`既定）に置く。`/tmp`配下はOS再起動で消えるため使わない。
- `export.sh`は`python3`のminor versionでvenv名（`idf5.4_py3.X_env`）を決める。Homebrew Pythonが更新されると既存venvが見つからず`export`が失敗するので、その場合は`./install.sh esp32s3`を再実行するか、`IDF_PYTHON_ENV_PATH`で既存venvを明示する。
- 毎回の手順:

```bash
source ~/esp/v5.4.4/esp-idf/export.sh
```


ESP-IDF `v5.4.4`をインストールしてexportしたターミナルで、リポジトリのルートから次を実行する。`/path/to/RoadWeave`は実際のcheckout pathに置き換える。

```bash
cd /path/to/RoadWeave/firmware
./scripts/check-toolchain.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

`/dev/cu.usbmodemXXXX`は接続したXIAO ESP32S3のport名に置き換える。接続前後の候補は次で確認できる。

```bash
ls /dev/cu.usbmodem*
```

## Windows（native PowerShell）

2026-09-06に現在のWindows機で導入・実機確認済み。[Windowsベンチ手順と結果](bringup/windows-bench-2026-09-06.md)を参照。
このPCではRepoルートから次で専用環境へ入れる（PowerShellのプロファイルやシステムPythonは変更しない）。

```powershell
Set-Location D:\RoadWeave
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
. .\firmware\scripts\enter-idf.ps1
```

ESP-IDF本体は`D:\Espressif\esp-idf-v5.4.4`、toolsは`D:\Espressif\tools`。
`enter-idf.ps1`は専用Python環境を選び、公式`export.ps1`を実行してversionを確認する。
別の設置先では`-EspressifRoot`、`-IdfPath`、`-PythonEnvPath`を指定できる。
Windowsの`.py`関連付けが別Pythonを指していても、`idf.py`はESP-IDF用Pythonで実行される。

EspressifのWindows向けESP-IDF環境を起動し、ESP-IDF `v5.4.4`がexportされたPowerShellで、次を実行する。`C:\path\to\RoadWeave`は実際のcheckout pathに置き換える。

```powershell
Set-Location C:\path\to\RoadWeave\firmware
.\scripts\check-toolchain.ps1
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

`COM5`は例であり、デバイスマネージャーの「ポート (COM と LPT)」に表示されるXIAO ESP32S3のCOM portへ置き換える。PowerShellの実行ポリシーでスクリプトが拒否される場合は、現在のPowerShellプロセスだけを対象にしてから再実行する。

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\scripts\check-toolchain.ps1
```

書き込み後にmonitorを終了する場合は、ESP-IDF monitorの通常の終了操作を使う。WSLへCOM portを転送して書き込む手順は、USBドライバ、port切断、monitorの再接続が環境依存になるため、この再現手順には含めない。

## Linux CI

GitHub Actionsでは、Linux runner上で公式の`espressif/esp-idf-ci-action@v1`を使い、`esp_idf_version: v5.4.4`、`target: esp32s3`、`path: firmware`を固定してbuildする。CIはLinux上のコンパイル再現性を確認するためのもので、実機へのflash、USB monitor、XIAOの8 MiB flash/PSRAM smoke testを代替しない。workflowは`.github/workflows/firmware.yml`にある。

## version確認

macOSでは`./scripts/check-toolchain.sh`、Windowsでは`.\scripts\check-toolchain.ps1`を最初に実行する。両方のスクリプトが`toolchain.lock`の`ESP_IDF_VERSION`と`idf.py --version`を照合し、versionが異なる場合はbuildへ進まない。
