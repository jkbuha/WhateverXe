# WhateverXe

[English](README.md)

WhateverXeは、一部のIntel Xe-LP内蔵GPU向けにAppleのグラフィックスドライバを適応させるLiluプラグインです。WhateverGreenを基に開発しています。

本プロジェクトは実験段階であり、[WhateverXe Graphics](https://github.com/b00t0x/WhateverXe-Graphics)の署名済みグラフィックスpkgが必要です。資材を`/Library`へ配置してAuxiliary Kernel Collection（AuxKC）を更新し、Systemファイルやsnapshotにはパッチを適用しません。WEXとグラフィックスpkgは、それぞれ独立したバージョンでリリースします。

## 対応範囲

単一のWEXバイナリと1つのグラフィックスpkgで、以下のmacOSに対応します。

| macOS | 必要バージョン |
| --- | --- |
| Big Sur | 11.7.11 |
| Monterey | 12.7.6 |
| Ventura | 13.7.8 |
| Sonoma | 14.8.9 |
| Sequoia | 15.8.x |
| Tahoe | 26.7以上 |

Big Sur〜Sonomaは検証済みカーネルの識別を必要とします。SequoiaとTahoeでは、実行時の命令列・ABI検査を維持した上で、同じOS系列内の更新を受け付けます。将来の更新が検証済みという意味ではありません。

WEXはOSのABIとGPUバックエンドを独立して選択します。OS別のグラフィックス資材と共通のAppleIntelTGLCompatはWhateverXe Graphicsが導入し、GPU世代・コネクタ配線への対応はWEXが担います。

| GPUファミリー | バックエンド実装があるdevice ID |
| --- | --- |
| Tiger Lake | `9A40`、`9A49`、`9A78` |
| Alder Lake-N / Twin Lake | `46D0`–`46D4` |

対応状況は機種やコネクタ構成によって異なります。この表はバックエンドの実装範囲を示すもので、すべての機種での動作を保証するものではありません。詳細な選択条件と上書き設定は[デバイス選択](docs/device-selection.md)を参照してください。

任意のRPL-P参照buildは、Tahoe上の物理`A7A0` revision 4と、厳密に対応する参照driver imageに限定されます。Type-C経路は引き続き実験段階です。他のlaptop機種やGPU IDでは、memory layout、ACPI、display routing、driver ABIを個別に検証する必要があり、このbuildが自動的に受け入れることはありません。

## 必須構成

[Releases](https://github.com/b00t0x/WhateverXe/releases)から **RELEASE** ZIPをダウンロードしてください。DEBUG ZIPは問題調査用です。各ZIPには`WhateverXe.kext`と対応するdSYMのみを収録し、両ZIPのチェックサムは`SHA256SUMS`で配布します。各版の変更点は[CHANGELOG](CHANGELOG.md)を参照してください。

| コンポーネント | 役割 |
| --- | --- |
| Lilu | WhateverXeの動作に必要なランタイム |
| WhateverXe.kext | OpenCoreでLiluより後に読み込む |
| [WhateverXe Graphics pkg](https://github.com/b00t0x/WhateverXe-Graphics/releases) | 署名済み資材を`/Library`へ導入しAuxKCを更新する |
| [AMFIPass v1.4.1 Mod](https://github.com/b00t0x/WhateverXe-Graphics/blob/main/extras/AMFIPass-v1.4.1-Mod-RELEASE.zip) | 改変されたグラフィックス構成に必要。OpenCoreで導入する |

SIPは`csr-active-config=03080000`（値`0x803`）に設定してください。SIPの全面有効化には対応していません。署名済みグラフィックスpkgとリンク先のMod AMFIPassを使用する場合、`amfi=0x80`と`ipc_control_port_options=0`は不要です。

Tahoeでは`CoreMedia / AllowOnePassMetalScaling`の無効化が必要です。WhateverXe Graphicsのインストーラーが設定します。

グラフィックスpkgはWEX **0.3.0以上**を読み込んだ対象OSから実行してください。旧Xe System patchがある場合は、先に元の復元手順で戻してください。pkgはEFI・Systemファイル・snapshot・SIP設定を変更しません。pkgの使用方法と単独のアンインストールスクリプトは[導入と削除](https://github.com/b00t0x/WhateverXe-Graphics/blob/main/README_ja.md#インストール)を参照してください。

リンク先の **Mod版** アーカイブから`AMFIPass.kext`を展開し、`EFI/OC/Kexts`へ配置して、`Kernel > Add`で有効にしてください。既存のAMFIPassがある場合はこのMod版へ置き換え、両方を同時に読み込まないでください。upstreamのAMFIPassはMod版の署名チェーンに対応していません。

他のroot patchにOCLPを併用する場合は、同じMod版の署名チェーンを使用する[独自ビルド版OCLP](https://github.com/b00t0x/OpenCore-Legacy-Patcher)を使用してください。Xeグラフィックスの導入自体はOCLPに依存せず、WhateverXe Graphicsを使用します。

複数GPUの構成を含め、WhateverGreenとWhateverXeは同時に有効にしないでください。

## 設定

GPUはPCI device IDから自動選択します。対応範囲内ではファームウェアのVBTから配線を読み取るため、固定HDMI／固定DisplayPort構成は通常、配線propertyの手動指定が不要です。高度な設定としての`device-id`上書きは[デバイス選択](docs/device-selection.md)で説明しています。

対応GPUでは、WhateverGreenと同様に、検出されたIntel Management Engineのデバイス名も`IMEI`へ統一します。`HECI`や`MEI`から`IMEI`へのACPI renameを別途追加する必要はありません。

### DVMT

BIOS/UEFIで変更できる場合は、**DVMT Pre-Allocatedを96MBに設定することを推奨します**。

**60MBから変更できない場合**は、`boot-args`に`-igfxdvmt`を追加し、OpenCoreの`DeviceProperties > Add`にあるiGPUの項目（通常は`PciRoot(0x0)/Pci(0x2,0x0)`）へ以下の2つを設定してください。

| Property | 型 | 値（16進数） |
| --- | --- | --- |
| `framebuffer-patch-enable` | Data | `01000000` |
| `framebuffer-stolenmem` | Data | `0000C003` |

Data値はリトルエンディアンです。`-igfxdvmt`は容量計算を補正し、DevicePropertiesは表示用メモリ領域を60MB内に収めます。ファームウェアが確保したメモリを増やす設定ではありません。96MBの場合、これらのDVMT用設定は不要です。

### 必須ブート引数

OpenCoreの`boot-args`に以下を追加してください。

```text
-allow3d -disablegfxfirmware
```

| 引数 | 用途 |
| --- | --- |
| `-allow3d` | Appleドライバの3D処理を有効にする。 |
| `-disablegfxfirmware` | 現在のバックエンドで、Appleドライバのグラフィックスファームウェア読み込みを無効にする。 |
| `-wex-nopanel` | 内蔵画面のないTiger Lake機で追加する。現在のAlder Lake-N / Twin Lakeは外部画面用の構成を自動生成するため、この引数を使用しない。 |

Alder Lake-N / Twin Lakeで世代選択や個別機能の有効化引数を追加する必要はありません。

### 任意のブート引数

| 引数 | 用途 |
| --- | --- |
| `gfxrst=` | 起動時framebufferの引継ぎ方式。0：自動（既定）、1：reset、2：copy、3：clear、4：無効。厳密なmachine判定を行うRPL-P参照backendでは、対応するIOGraphicsFamilyの明示的な `gfxrst=1` のみ有効です。その他の値では引継ぎを変更しません。 |
| `-wexdbg` | debug buildでログを有効にする。 |
| `-wexoff` | WhateverXeを無効にする。 |
| `-wexbeta` | プラグインのOSバージョンチェックを回避する。kernel／driverの一致条件は維持する。 |
| `-wex-rplp-probe` | Tahoe上の物理Raptor Lake-P A7A0 revision 4向けGPU fuse／engineおよび電源が入ったpipeとファームウェアメモリの読み取り専用診断。時間制限付きGT／render forcewake取得で情報を収集します。アクセラレーションの有効化やディスプレイの設定変更は行いません。 |
| `-wex-rplp-bcs` | Tahoe上の物理A7A0 revision 4向け、時間制限付きcopy engineコマンドの実験的診断。`-wex-rplp-probe`と検証済みファームウェアメモリ引き継ぎが必要です。テスト用contextを確保して再起動まで保持します。通常のアクセラレーションは有効化しません。 |
| `-wex-rplp-rcs` | Tahoe上の物理A7A0 revision 4向け、時間制限付きrenderコマンドの実験的診断。`-wex-rplp-probe`が必要で、`-wex-rplp-bcs`との併用はできません。テスト用contextを再起動まで保持し、通常のアクセラレーションは有効化しません。 |
| `-wex-rplp-native-probe` | Tahoe上の物理A7A0 revision 4向けnative accelerator probeの実験的診断。`-wex-rplp-probe`が必要で、command probeとは別に使用します。正確な署名済みdonorを検証してdevice選択を許可し、GPU初期化前にnative `start()`を停止します。 |
| `-wex-rplp-native-memory` | nativeメモリマッピングの実験的診断。RPL-Pの両probe引数が必要です。native BAR mappingとaddress segmentを初期化し、dummy-page割当およびGGTT初期化の前に意図的に失敗させます。アクセラレーションは無効のままです。 |
| `-wex-rplp-native-pages` | native dummy-page割当の実験的診断。RPL-Pの両probe引数と`-wex-rplp-native-memory`が必要です。割当失敗時のcleanupを保護し、両CPU bufferを検証してGGTT初期化前に停止します。 |
| `-wex-rplp-gtt-shadow` | 所有するRAM上でnative GGTT constructorを検証する実験的診断。RPL-Pのnative probe、memory、page引数がすべて必要です。予約prefix、64-bit PTE storeとcleanupを検証し、別途slot診断を要求しない限り実際のGGTTは変更しません。 |
| `-wex-rplp-cache-transaction` | 103個のcache-policy registerの書き込み・読み取り確認・復元を行う実験的診断。cache-shadow成功、idleな96-EU/GOP状態、管理者によるmanual armが必要です。firmware MOCS61、PAT0とL3 layoutを保持し、試行した書き込みをすべて復元します。renderingやaccelerationは有効にしません。 |
| `-wex-rplp-caps-shadow` | 実験的な読み取り専用capability取得と、所有RAM上でのnative capability/mode再生。cache-shadow/cache-transactionの成功とmanual armが必要です。hardware engine modeやaccelerationは有効にしません。 |
| `-wex-rplp-native-engines` | native ring-field/mode/HWS helperと検証済みGen12 register templateを使う実験的render/blitter command診断。native-memory/capability診断の成功とmanual armが必要です。別々のGGTT範囲を使い、submit済みcontextは再起動まで保持します。shaderやmacOS accelerationは有効にしません。 |
| `-wex-rplp-donor-fixes` | 実験的なRPL-P専用donor修正。context電源、preemption、Tahoe plane数、圧縮policyを調整します。manual armとnative-engine診断が必要です。accelerator起動は引き続き無効です。 |
| `-wex-rplp-reference-backend` | 専用buildのRPL-P参照backendを有効にします。厳密なmachine判定とroot manual armが必要です。通常のTGL/診断backendは起動しません。 |
| `-wex-rplp-fb` | 厳密な参照framebuffer donorを指定した専用buildでRPL-P panel backendを有効にします。参照backendとroot manual armが必要です。通常の署名済みframebufferではこれらのhookを使用できません。 |
| `-wex-rplp-native-barriers` | native accelerator の公開後に依存 barrier を有効にする実験的 RPL-P 診断。任意の panel backend と手動 arm が必要で、native context を検証してから従来の startup path を離れます。 |
| `-wex-rplp-backlight` | 任意の RPL-P panel backend 向けに UID-19・16-bit PWM の輝度 profile を有効にします。Apple または custom profile を保持し、外部 display は対象外です。 |
| `-wex-rplp-tcss` | 任意の RPL-P panel backend 向けの実験的な ADL-P/RPL-P Type-C 検出です。4 個の TC port slot を公開し、TCSS の ready/DP-alt 状態を使用します。Thunderbolt transport は対象外で、外部 link training は検証中です。 |
| `-wex-rplp-tcss-aux-probe` | RPL-P TCSS observer と併用し、特権 snapshot から TC3 PHY/AUX 電源と受信機 capability の読み取りを 1 回だけ検証します。取得した電源・ownership request は終了後に戻し、video link training は行いません。 |
| `-wex-rplp-tcss-live` | TCSS observer と AUX probe を併用する実験的な TC3 専用 native link training。特権 AUX snapshot の成功後にだけ経路を許可し、他の外部 port は未接続として保持します。Native AUX 参照数を維持し、ADL-P/RPL-P の ownership sequence を使用します。 |
| `-wex-rplp-tcss-observe` | 実験的 RPL-P TCSS backend で外部 port の dispatch と mode 検出を記録し、外部 port は未接続として保持します。別の live 試験を明示的に選択しない限り、link training は無効です。 |
| `-wex-rplp-early-display` | 実験的な RPL-P 起動制御です。検証済み owner の firmware framebuffer binding を遅延し、native framebuffer で最初の desktop を表示します。早期 activation coordinator と recovery 設定が必要です。 |
| `-wex-rplp-gpu-recovery` | 対応する reference GPU image 向け RPL-P shared render-domain reset recovery。native accelerator の公開後、native reset で状態を失った sibling engine を検証して復元・再実行します。 |
| `-wex-rplp-gpu-recovery-v2` | `-wex-rplp-gpu-recovery` の実験的な拡張です。shared render-domain reset 後、command replay の前に compute engine の有効状態と関連 register を復元します。両方の引数と対応する reference image が必要です。 |
| `-wex-rplp-blit-pitch` | 対応する row stride が 64 byte の倍数でない大きな IOSurface copy を copy engine に送る実験的な設定です。対応する RPL-P reference image と native accelerator の ownership が必要です。 |
| `-wex-rplp-forcewake-balance` | 対応する reference GPU image 向け RPL-P forcewake counter balancing。idle hold の自動解放は無効です。 |
| `-wex-rplp-manual-arm` | 管理者が`WhateverXeRPLPControl` serviceへ`BeginDiagnostic=true`を送信するまでRPL-P native診断を待機させます。既存のnative-probe前提条件が必要です。実験的診断の開始前に動作確認済みboot fileを復元できます。通常のaccelerator admissionは有効にしません。 |
| `-wex-rplp-subranges` | native descriptorの割り当て・枯渇・backing RAMの共有・復元・解放と再利用、およびcustom eventを使わないpage-pool登録を検証する実験的診断。factory全体とcache-shadow診断の成功が必要です。所有RAMのみを使用し、子descriptorの所有権が不明な場合は再起動までmanagerを保持します。accelerationは有効にしません。 |
| `-wex-rplp-cache-shadow` | 所有RAM上でnative cache初期化を再生する実験的診断。模擬forcewakeの取得・解放とRAM全体・境界を検証します。native-factory診断が必要です。時間制限付きforcewake中に既存cache registerも読み取りますが、実機cacheへの書き込みやaccelerationの有効化は行いません。 |
| `-wex-rplp-native-factory` | memory factory全体の構築とcleanupを検証する実験的診断。統合manager-pool診断が必要です。capabilityの連続した読み取りでeDRAM無効を確認した場合のみ先へ進み、旧forcewake probeの代わりに測定結果を使います。最後にpool arrayとmanagerを明示的に解放します。acceleratorの起動やGT cacheのprogrammingは行いません。 |
| `-wex-rplp-manager-pools` | 実際のnative manager内でpool arrayの構築とcleanupを検証する実験的診断。page-array、空のpage-pool診断およびその前提引数が必要です。manager内にarrayを構築した後、通常は元の失敗時unwindを強制します（`-wex-rplp-native-factory`指定時を除く）。cache programmingは行いません。accelerationは有効にしません。 |
| `-wex-rplp-page-array` | 空のpage-pool診断の成功後に、CPUごとのnative page-pool arrayを検証する実験的診断。専用のCPUメモリ上のmanager record、割当失敗を処理するpool constructor、修正した部分失敗cleanupを使用し、最後にarrayを解放します。完全なmemory managerやGPU engineは有効にしません。 |
| `-wex-rplp-page-pool` | 空のnative page-pool constructorを検証する実験的診断。native-chainとnative-pools診断の成功が必要です。割当失敗を処理し、空のcontainer metadataを検証してobjectを解放します。memory-managerのpage-pool gateは停止したままで、GPU engineやaccelerationは有効になりません。 |
| `-wex-rplp-native-chain` | slot/aperture検証後にmemory managerの構築を進める実験的診断。native pool診断の前提引数がすべて必要です。所有するRAMをnative poolに渡してfactory内でpoolとfence allocatorを構築し、通常はpage-pool初期化の前で停止します（`-wex-rplp-manager-pools`指定時はarray構築後、cache programming前で停止）。cleanupを検証してからbacking RAMを解放します。物理stolen memoryの割当やGPU engineの有効化は行いません。 |
| `-wex-rplp-native-pools` | aperture診断の成功後にnative allocatorの構築を検証する実験的診断。所有するRAM 1 pageをnative stolen-memory poolのbackingとして使い、そのdescriptorとnative fence allocatorを検証します。native objectを解放してからbacking pageを解放します。物理stolen memoryの割当、fenceのprogramming、GPU commandの投入は行いません。 |
| `-wex-rplp-gtt-aperture` | bounded GGTT slot診断中にapertureから読み出す実験的検証。`-wex-rplp-gtt-slot`とその前提引数がすべて必要です。native dummy pageをflushし、uncached aperture mappingから全1024 wordを検証します。不一致時もfirmware PTEを復元します。GPU commandやapertureへの書き込みは行いません。 |
| `-wex-rplp-gtt-slot` | GGTTの1 entryだけを対象にする実験的hardware診断。shadow、pin-pagesおよびすべてのnative probe引数が必要です。idle GTとpanel handoffを再検証し、active scanoutの外にある10MiBのspare pageのidentity mappingに限定してnative書き込みを検証し、元のPTEを復元します。復元失敗または別entryの変化時はbufferを次のrebootまでpinしたまま保持します。acceleratorの開始やGPU commandの投入は行いません。 |
| `-wex-rplp-pin-pages` | `-wex-rplp-gtt-shadow`実行時のbuffer lifetime診断。追加のprepare/retainで保持し、native cleanup後にpreparation IDと物理addressの安定性を検証して追加保持を解放します。 |
| `-wex-tgl-off` | Tiger Lakeの処理を無効にする。 |
| `-wex-noaccel` | Tiger LakeのFramebuffer単独モード。Acceleratorのprobeを停止します。 |
| `-wex-edp-probe` | 内蔵パネルのAUX／DPCDを範囲を限定して診断します。検証済みパネル経路では`-wex-edp-routing`と併用する必要があります。この引数だけではパネルを有効にしません。 |
| `-wex-edp-routing` | Tahoe上の物理Tiger Lake 9A49 revision 1向けの実験的なパネルポート経路、トレーニング前のクロック選択、native画面／AUX電源制御、Combo PHY／2レーンのトレーニング、送信側フレーミング、null hotplugパスの保護。`-wex-edp-probe`とdonor命令の完全一致を必要とします。 |

## 動作確認済みの機能と既知の問題

Tiger LakeとAlder Lake-Nで、全6 OSにわたり、外部2画面の1920×1080・60Hz出力、Metal/MPS、OpenGL、OpenCL、ハードウェアH.264エンコード／デコード、ハードウェアHEVCデコード、画面の消灯／復帰、Safariのハードウェア動画デコード、Widevine再生を確認しています。共通TGLCompatビルドも、Alder Lake-Nの全6 OSで導入と機能を確認しています。すべてのcodec・アプリケーション・表内のGPUでの互換性を確認したものではありません。

物理Tiger Lake `9A49` revision 1向けの検証済みTahoe経路では、内蔵1920×1200パネルの起動と、GPUバッファの読み戻しを伴うMetal 2コマンドの完了も確認しています。検証したnative描画圧縮ポリシーは、起動時と以後のplatform更新時に無効を維持します。この内蔵パネル構成での外部画面の抜き差し、システムスリープ／復帰、長時間の安定性は未検証です。

### 既知の問題と制限

- VA画像変換でnull参照によるクラッシュが発生することがあります。
- Sequoia・Tahoeなどで、DisplayPortの再接続により黒画面やシステムハングが発生することがあります。
- Safariでは少数のフレーム落ちを確認しています。
- 3画面同時出力、システムスリープ、より高い解像度や他のコネクタ構成は追加検証が必要です。USB-Cと内蔵ディスプレイへの対応は下記ロードマップに含まれます。

## ロードマップ

### Planned

- Alder Lake／Raptor LakeのS/H/P/U対応
- USB-C画面出力対応
- 内蔵ディスプレイ対応

### Not Planned

- Xe-LPG対応
- Xe2以降のアーキテクチャへの対応

## ビルドとテスト

macOS、Xcode command-line tools、Python 3.9以降、MacKernelSDK、Liluの開発用ヘッダ／ライブラリを使用します。CIではTahoeのkernel定義を含むLiluの[1.7.2リリースタグ](https://github.com/acidanthera/Lilu/tree/1.7.2)を使用します。`MacKernelSDK/`と`Lilu.kext/`をリポジトリ直下に配置するか、`--sdk`と`--lilu`でパスを指定してください。

```sh
python3 tools/build.py --release
python3 tools/test.py
```

WhateverXeとAppleIntelTGLCompatのkext、およびソースのmanifestは`build/`に出力されます。TGLCompatのソースと共有ヘッダーはこのリポジトリで管理し、署名済みバイナリはWhateverXe Graphicsで配布します。WEXのRelease ZIPに含まれるkextはOpenCore用プラグインのみで、グラフィックスpkgは含みません。任意のdonor再生試験には`--donor /path/to/AppleIntelTGLGraphicsFramebuffer`と`--accelerator /path/to/AppleIntelTGLGraphics`を指定します。各試験が受け付ける正確なバイナリが必要です。CPUテストだけで実GPUの互換性が確認できるわけではありません。ビルドにWhateverGreenのソースは不要です。

## ライセンスとクレジット

WhateverXeはWhateverGreenの著作権表示とBSD-3-Clauseライセンスを継承しています。[LICENSE.txt](LICENSE.txt)を参照してください。LiluとMacKernelSDKは別の依存コンポーネントです。

任意のRPL-P参照実装統合: commit `d0c046a441fa467ac492770aeeefe9636b97c65e` のcheckoutを `--rplp-reference-source /path/to/Hackintosh-XPS13Plus-IrisXe-Tahoe` で指定します。この実験的buildは別ライセンスのTSNPL sourceを含み、build出力にnoticeを保存します。BSDのみの配布物ではありません。通常buildはこのsourceを含みません。任意の `--rplp-reference-framebuffer /path/to/AppleIntelTGLGraphicsFramebuffer.kext` で厳密に対応するpanel donorの契約を追加できます。Apple binaryは別のbuild入力として保持します。

Panel buildは、system graphicsの依存先が利用可能になってから参照framebufferを読み込むための `WhateverXeRPLPBridge.kext` も生成します。このproviderはkernel KPIだけに依存し、6種類の操作をWhateverXeが検証したframebuffer ownerへ転送します。読み込みだけではGPUのarmやnative framebufferのmatchingを有効にしません。

パネルバックエンドは手動アーミング後に、署名済みパーソナリティ本来のプローブ優先度で IOKit マッチングを行います。ディスプレイパイプの作成には、このバックエンドが所有するネイティブフレームバッファが必要です。
