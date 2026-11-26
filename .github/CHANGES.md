# mainlineからの変更

ベース: torvalds/linux 7.4-rc4。変更はすべて`origin/master..master`のコミット。

## 動作状況

- 動作: CPU 4コア、cpufreq(300〜1100 MHz、ondemand、CPU電圧0.8〜0.9625 Vも連動)
- 動作: eMMC(HS200 190 MHz)
- 動作: SDカード(HS 50 MHz)
- 動作: USB 3.0(SuperSpeed)/ USB 2.0
- 動作: Wi-Fi RTL8192EE(2.4 GHz)/ RTL8812AE(5 GHz)
- 動作: 温度センサー、ファン、LED、ボタン、watchdog、乱数、eFuse
- 動作: KVM
- 未検証: SATA(2.5インチディスクが無く確認できていない)

## 起動まわりの制約と理由

- **Imageヘッダのtext_offsetを0x100000にする(`CONFIG_QUASTATION=y`)**
  mainlineではtext_offsetは廃止されたがQua StationのU-BootはImageを「DRAM先頭 + text_offset」にコピーしてから起動するため、0x100000に設定しないとU-Bootが上書きされてハングする。
- **起動時の`WARNING: could not set bootargs FDT_ERR_NOSPACE`は無視してよい**
  1段目U-Bootがdtbにbootargsを書こうとして失敗するだけで、純正でも出る。実際にbootargsを入れるのは2段目U-Bootで、dtbを0x3000バイト広げてから書くので、dtbに空きを付ける必要はない。
- **2〜4コア目は独自のenable-method(`realtek,rtd1295-spin-table`)**
  ファームウェア(BL31 v1.2)にPSCIが無く、2次コアはEL3で32bitのSB2レジスタを見て待っている。汎用のspin-tableはEL2/EL1と64bitのメモリ書き込みが前提で使えない。

## 大まかなコミット内容

### mainlineのバグ修正(Qua Station以外にも影響する)

- `gpio: rtd: don't claim the shared interrupt window on RTD1295` — RTD129xではGPIOの割り込みレジスタ窓が上流DTのISOリセットコントローラ(0x88)と必ず重なり、-EBUSYでプローブに失敗していた
- `gpio: rtd: add the GPIO IRQ resource helpers to the irq_chip` — GPIO割り込みを要求するたびにWARNが出ていた
- `phy: realtek: usb3: fix duplicated register in the RTD1295 table` — 表の先頭でレジスタ0x01が2回書かれ、0x00が未設定だった
- `phy: realtek: usb2: fix the RTD1295 page 0 E2/E3 values` — ベンダーBSPの値と違っていた
- `watchdog: rtd119x: add a restart handler` — PSCIが無いので再起動手段が無かった
- `watchdog: rtd119x: advertise settimeout and keepalive ping` — systemdなどがpingできず、120秒でリセットされていた
- `wifi: rtlwifi: serialize IPS interrupt enable/disable with the ISR` — 省電力(IPS)の出入りと割り込みハンドラが競合し、レベル割り込みが鳴りっぱなしになってCPUがストールしていた

### RTD1295の新規ドライバ

- `pinctrl`、`irqchip`(ISO/MISC割り込み多重化)、`clk`(ゲート、PLL、cpufreq用SCPUクロック)
- `nvmem`(eFuse)、`thermal`、`hwmon`(ファン回転数)、`hwrng`
- `phy`(EHCI/OHCI用USB2 PHY、SATA PHY ※未検証)
- `soc: realtek: add RTD1295 A01 rbus erratum lock` — A01ではeMMCのレジスタアクセスが他のバスマスタのアクセスで壊れることがあり、eMMCとPCIeでロックを共有する
- `PCI: rtd1295` — PCIe 2スロット。CPUからPCIのMEM窓ではカードのレジスタに届かないため、コントローラの4 KiB窓 +変換レジスタ経由のアクセサを提供
- `mmc: rtd1295 eMMC` — 汎用dw_mmcはエラー後のリセットでSoCごと固まるため、ベンダーと同じ手順で動かす。HS200(190 MHz)対応
- `mmc: rtd1295 SD` — SDHCIでもDW MSHCでもない独自コア。HS 50 MHz
- `regulator: add GMT G2227 PMIC driver` — PMIC(DCDC×6、LDO×2)。CPU電圧(dcdc2)をcpufreqに連動させる。起動ごとに最初のI2C転送だけNAKされる(原因は起動の流れ側と推定)ため、チップIDの読み出しを再試行する

### 既存ドライバへのRTD1295向け追加

- `wifi: rtlwifi: pci: use the RTD1295 PCIe window accessors` — 上のPCIe窓経由でカードにアクセスする
- `usb: dwc3: rtk: route the RTD1295 SuperSpeed lanes on USB-A boards` — USB3のSS信号がType-Cの切り替えスイッチを通るが、USB-Aの基板では誰も設定しないため
- `usb: dwc3: rtk: bring up the RTD1295 USB resets and clock` — U-Bootで`usb start`しなくてもUSBが動くように、ベンダーと同じ順序でリセットとクロックを立ち上げる

### Qua Station専用(`CONFIG_QUASTATION`)

- `arm64: add CONFIG_QUASTATION for the Qua Station vendor U-Boot` — text_offset 0x100000
- `arm64: add the RTD1295 spin-table enable method for the Qua Station` — 2〜4コア目の起動

### ボード

- `arm64: dts: realtek: add the au Qua Station` — DT
- `arm64: configs: add a Qua Station config fragment` — `make defconfig quastation.config`
- `arm64: dts: realtek: quastation: add the G2227 PMIC` / `arm64: configs: quastation: enable the G2227 regulator` — G2227をCPUの電源(`cpu-supply`)にする

## 既知の問題

- cpufreqは1.1 GHzまで(1.2 GHz以上はL2の電圧(ldo3)もCPU電圧に合わせて上げる必要があり未対応)
- G2227はdcdc2の電圧変更だけ実機確認済み。ON/OFF、モード切り替え、他の系統の電圧変更は未検証
- schedutilではアイドル時に周波数が下がらないため、既定のガバナーはondemand
- Wi-Fiカードは自前の設定値を持たず(`Autoload ERR`)、MACアドレスは起動ごとにランダム。送信出力の補正値も使っていない(どちらも純正ではeMMCの工場設定領域から読んでいた？)
- RTL8812AEからは2.4 GHzのAPが見えない
- 起動時に`Kernel image misaligned at boot`と出る(text_offset 0x100000が2 MiB境界でないため。動作に影響なし)
- 新規のcompatibleの多くはDT binding(YAML)が未作成
