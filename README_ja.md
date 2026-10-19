# WhateverXe

[English](README.md)

WhateverXeは、一部のIntel Xe-LP内蔵GPU向けにAppleのグラフィックスドライバを適応させるLiluプラグインです。WhateverGreenを基に開発しています。

本プロジェクトは実験段階であり、[OpenCore Legacy Patcherのfork](https://github.com/b00t0x/OpenCore-Legacy-Patcher)が提供するXe root patchを必要とします。

このforkは、Big Sur `20G1443`上の物理Tiger Lake `9A49` revision 1向けに、実験的なDell XPS 9310ディスプレイ対応を追加します。検証した構成では、内蔵1920×1200画面、**右側USB-Cポート**のUSB-C–HDMIアダプタ経由の外部1920×1080画面との同時出力、ケーブルなしの起動、抜き差しが動作しました。初期の実機検証結果であり、長時間の安定性とシステムスリープ／復帰は未検証です。以下のupstreamリリースZIPには、このforkの変更は含まれません。

## XPS 9310のディスプレイ変更

動作する表示経路は、次の3つの変更群を組み合わせたものです。

1. **VNCのみの状態から外部HDMI出力へ。** HDMIアダプタはUSB-CのDisplayPort Alt Modeを使います。Tiger Lake経路に、対象を限定したType-C電源管理、実測した2レーンpin-D配線、pipe／transcoderの経路補正、ネゴシエーション結果に合わせたDisplayPort enhanced framingの補正を追加しました。外部画面への実出力が可能になりましたが、初期段階ではファームウェア起動時にFn+F8で外部画面を選択する必要がありました。
2. **内蔵パネルの出力。** eDP経路に、AUX電源を確保したEDID／パネル識別、内蔵pipe／DDI経路、パネルクロックと電源シーケンスの補正、リンクトレーニング時のSST enhanced framing整合を追加しました。描画側では、大きな診断用一時バッファをnativeドライバ呼び出し前に解放し、texture viewの実際のbacking storageを検証します。厳格な条件下で初回使用前のC0 texture圧縮を回避して描画破損に対処しました。最初の実用的な内蔵画面構成は、まだHDMI画面の接続に依存していました。
3. **内蔵と外部の同時出力、およびケーブルなしでの使用。** 右ポートのlane getterを、donorのFIA1参照からmodular **FIA2 / TC3**参照へ修正しました。特定のnative信号設定呼び出し箇所に限定して両方のDKL送信バンクを初期化し、Fn+F8なしで内蔵画面と外部リンクを同時に有効化します。ケーブルなしでの描画には、固定サイズ一覧ではなく上限付きnative texture layout検証でC0圧縮回避を適用し、ページ切り上げ前にnativeの**192バイトの補助領域末尾**を割当サイズへ含めます。初回圧縮有効化前の未使用CCSサイズだけを変更し、未使用状態、所有関係、共有storage、正確な割当サイズを検証します。既に圧縮されたtextureの再解釈は行いません。

接続状態での起動、Safari使用、HDMIの抜き差し、HDMIなしの起動を確認しています。他のアダプタ、モニタ、解像度、左USB-Cポートは別途検証が必要です。物理デバイスとnative Appleドライバのguardは維持しており、Raptor Lake-P／XPS 9320対応を追加するものではありません。

## 対応範囲

現在の対象は **macOS Big Sur build 20G1443** です。

| GPUファミリー | バックエンド実装があるdevice ID |
| --- | --- |
| Tiger Lake | `9A40`、`9A49`、`9A78` |
| Alder Lake-N / Twin Lake | `46D0`–`46D4` |

対応状況は機種やコネクタ構成によって異なります。この表はバックエンドの実装範囲を示すもので、すべての機種での動作を保証するものではありません。詳細な選択条件と上書き設定は[デバイス選択](docs/device-selection.md)を参照してください。

Tiger LakeとAlder Lake-Nで、外部2画面の1920×1080・60Hz出力、Metal/OpenGL、ハードウェアHEVC、画面の消灯／復帰を確認しています。Tiger LakeではハードウェアH.264のエンコード／デコードも確認しています。3画面同時出力、システムスリープ、より高い解像度や他のコネクタ構成は追加検証が必要です。上記XPS 9310の結果は検証した内蔵／USB-C経路に限定され、より広いUSB-Cと内蔵画面への対応はロードマップに含まれます。

[Releases](https://github.com/b00t0x/WhateverXe/releases)から **RELEASE** ZIPをダウンロードしてください。DEBUG ZIPは問題調査用です。

## 必須構成

| コンポーネント | 役割 |
| --- | --- |
| Lilu | WhateverXeの動作に必要なランタイム |
| WhateverXe.kext | OpenCoreでLiluより後に読み込む |
| [OpenCore Legacy Patcherのfork](https://github.com/b00t0x/OpenCore-Legacy-Patcher) | Xe root patchを適用してグラフィックス構成を導入する |
| [AMFIPass v1.4.1 Mod](https://github.com/b00t0x/OpenCore-Legacy-Patcher/blob/mod/payloads/Kexts/Acidanthera/AMFIPass-v1.4.1-Mod-RELEASE.zip) | 改変されたroot patchのグラフィックス構成に必要。OpenCoreで導入する |

OCLP forkは[Releasesページ](https://github.com/b00t0x/OpenCore-Legacy-Patcher/releases)からダウンロードしてください。rootの構成要素の導入とkernel collectionの更新はOCLPが行います。

リンク先の **Mod版** アーカイブから`AMFIPass.kext`を展開し、`EFI/OC/Kexts`へ配置して、`Kernel > Add`で有効にしてください。既存のAMFIPassがある場合はこのMod版へ置き換え、両方を同時に読み込まないでください。

複数GPUの構成を含め、WhateverGreenとWhateverXeは同時に有効にしないでください。

## 設定

GPUはPCI device IDから自動選択します。対応範囲内ではファームウェアのVBTから配線を読み取るため、固定HDMI／固定DisplayPort構成は通常、配線propertyの手動指定が不要です。高度な設定としての`device-id`上書きは[デバイス選択](docs/device-selection.md)で説明しています。

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
| `gfxrst=` | 起動時framebufferの引継ぎ方式。0：自動（既定）、1：reset、2：copy、3：clear、4：無効。 |
| `-wexdbg` | debug buildでログを有効にする。 |
| `-wexoff` | WhateverXeを無効にする。 |
| `-wexbeta` | プラグインのOSバージョンチェックを回避する。kernel／driverの一致条件は維持する。 |
| `-wex-tgl-off` | Tiger Lakeの処理を無効にする。 |
| `-wex-tc-bootstrap` | 開発用試験。起動中はTC-cold電源を保持する。このソースツリーでは、物理Tiger Lake `9A49`のDDI5に限定した2レーン・pin-DのDP Alt Mode処理も、画面電源を保持して有効にする。実験段階であり、システムスリープや一般的なUSB-C対応は未検証。 |
| `-wex-edp-probe` | 物理Tiger Lake `9A49`用の診断試験。フレームバッファ起動後、既存ドライバのVDD/AUX電源を一時保持して内蔵パネルのDPCDとEDIDを読む。診断プロパティのみ公開し、内蔵画面出力は有効にしない。 |
| `-wex-edp-routing` | 物理Tiger Lake `9A49`の内蔵画面用実験。`-wex-edp-probe`が必要。検証済みのeDP処理で実際のパイプを選び、AUX転送中のVDDを管理し、未確定の起動モードを拒否する。実機検証は未完了。 |

## ロードマップ

### Planned

- OCLPの機能を利用したmacOS Monterey〜Tahoe対応
- Alder Lake／Raptor LakeのS/H/P/U対応
- USB-C画面出力対応
- 内蔵ディスプレイ対応

### Not Planned

- Xe-LPG対応
- Xe2以降のアーキテクチャへの対応

## ビルドとテスト

macOS、Xcode command-line tools、Python 3.9以降、MacKernelSDK、Liluの開発用ヘッダ／ライブラリを使用します。`MacKernelSDK/`と`Lilu.kext/`をリポジトリ直下に配置するか、`--sdk`と`--lilu`でパスを指定してください。

```sh
python3 tools/build.py --release
python3 tools/test.py
```

kextとソースのmanifestは`build/`に出力されます。任意のdonor再生試験には`--donor /path/to/AppleIntelTGLGraphicsFramebuffer`と`--accelerator /path/to/AppleIntelTGLGraphics`を指定します。各試験が受け付ける正確なバイナリが必要です。CPUテストだけで実GPUの互換性が確認できるわけではありません。ビルドにWhateverGreenのソースは不要です。

## ライセンスとクレジット

WhateverXeはWhateverGreenの著作権表示とBSD-3-Clauseライセンスを継承しています。[LICENSE.txt](LICENSE.txt)を参照してください。LiluとMacKernelSDKは別の依存コンポーネントです。
