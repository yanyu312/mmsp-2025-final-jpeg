# MMSP 期末專題 — JPEG-like Encoder / Decoder

本專題依照課程規格，以 C 語言實作一個簡化版 JPEG-like 影像壓縮與解壓縮系統，完成 **Method 0** 與 **Method 1**。

---

## 一、專題內容概述

本系統包含兩個主要程式：

- `encoder.c`
- `decoder.c`

支援兩種操作模式：

- **Method 0**：BMP 影像讀寫與 RGB 色彩通道分離 / 重建  
- **Method 1**：JPEG-like 壓縮流程（不包含 entropy coding）

---

## 二、支援影像格式

- 輸入影像需為：
  - 24-bit BMP
  - 未壓縮（BI_RGB）

本專題使用課程提供之測試影像 **`Kimberly.bmp`** 進行測試與驗證。

---

## 三、Method 0：RGB 基本處理

### Encoder（Method 0）

```bash
encoder 0 Kimberly.bmp R.txt G.txt B.txt dim.txt
功能說明：
-讀取 BMP 圖檔
-將影像分離為 R / G / B 三個通道
輸出：
-R.txt、G.txt、B.txt
-dim.txt（影像寬度與高度）

Decoder（Method 0）
decoder 0 Res0.bmp R.txt G.txt B.txt dim.txt
功能說明：
-從 RGB 文字檔與影像尺寸資訊
-重建並輸出 BMP 圖檔
```
## 四、Method 1：JPEG-like 壓縮流程
```
### Encoder（Method 1）
encoder 1 Kimberly.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt
qF_Y.raw qF_Cb.raw qF_Cr.raw eF_Y.raw eF_Cb.raw eF_Cr.raw

### 處理流程說明
- 讀取 BMP 圖檔
- RGB → YCbCr 色彩空間轉換
- Level Shift（每個像素值減去 128）
- 將影像切分為 8×8 區塊（邊界以複製方式補齊）
- 對每個區塊進行 2D DCT
- 使用 JPEG 標準量化表進行 Quantization
- 輸出以下檔案：
  - 量化表：Qt_Y.txt、Qt_Cb.txt、Qt_Cr.txt
  - 量化後係數：qF_*.raw（int16）
  - DCT 係數：eF_*.raw（float32）
  - 影像尺寸：dim.txt
```
## 五、Decoder（Method 1）
```
decoder 1 Res1.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt
qF_Y.raw qF_Cb.raw qF_Cr.raw


### 解碼流程說明
- 讀取影像尺寸
- 讀取量化表
- 讀取量化後 DCT 係數
- De-quantization
- Inverse DCT（IDCT）
- Inverse level shift（加回 128）
- YCbCr → RGB 色彩轉換
- 重建並輸出 BMP 圖檔
```
## 六、注意事項
```
- 採用 8×8 區塊為 JPEG 標準設計
- 邊界補齊使用 edge replication
- 由於量化關係，重建影像可能出現輕微失真
- 本專題未實作 entropy coding（如 Huffman coding）
```
## 七、編譯方式
```
使用 Windows + MinGW GCC：

gcc encoder.c -o encoder.exe -lm
gcc decoder.c -o decoder.exe -lm
```
## 八、程式使用方法
```
### Encoder - Method 0
encoder 0 Kimberly.bmp R.txt G.txt B.txt dim.txt

### Decoder - Method 0
decoder 0 ResKimberly.bmp R.txt G.txt B.txt dim.txt

### Encoder - Method 1
encoder 1 Kimberly.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt
qF_Y.raw qF_Cb.raw qF_Cr.raw eF_Y.raw eF_Cb.raw eF_Cr.raw

### Decoder - Method 1(a)
decoder 1 QResKimberly.bmp Kimberly.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt
qF_Y.raw qF_Cb.raw qF_Cr.raw

### Decoder - Method 1(b)
decoder 1 ResKimberly.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt
qF_Y.raw qF_Cb.raw qF_Cr.raw eF_Y.raw eF_Cb.raw eF_Cr.raw

---
## 系統架構（Block Diagram）

BMP → RGB → YCbCr → Level Shift → 8×8 Blocking → DCT → Quantization → qF/eF  
qF → Dequant → IDCT → YCbCr → RGB → BMP

---

## 工作日誌（Work Log）
- 12/22：完成 BMP 讀寫與 RGB 分離（Method 0）
- 12/23：實作 YCbCr、8×8 DCT、Quantization（Method 1 encoder）
- 12/24：完成 decoder、支援 method 1(a)/1(b)，整合 GitHub Actions

---

## 心得與反思

本專題讓我實際從程式層面理解 JPEG 壓縮流程，  
包含色彩空間轉換、區塊式 DCT 與量化對影像品質的影響。  
在實作 decoder 時，對參數規格與流程對齊有更深刻體會，  
也學習如何利用 CI workflow 讓程式能被自動驗證與重現。

---

## GitHub Workflow 說明

本專案使用 GitHub Actions：
- 自動下載 Kimberly.bmp
- 編譯 encoder / decoder
- 執行 Method 0 與 Method 1
- 將輸出結果上傳為 artifact 供評分使用
```
## 九、檔案結構
```
mmsp_final/
├─ encoder.c
├─ decoder.c
├─ README.md
└─ Kimberly.bmp
```
## 十、結論
```
本專題成功實作一個 JPEG-like 影像壓縮與解壓縮系統，  
完整涵蓋色彩空間轉換、8×8 區塊 DCT、量化與重建流程，  
並能正確處理課程提供之 BMP 測試影像。
