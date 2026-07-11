# 手勢音階互動樂器

這是一個適合平板使用的九宮格手勢音樂互動網頁。使用者透過攝影機辨識九種手勢，演奏固定音高的 Do、Re、Mi、Fa、Sol、La、Si、高音 Do、高音 Re，並搭配玻璃、水滴與空靈聲場效果。

目前 v1 成功版已完成整條系統鏈：筆電／iPad／iPhone 從 GitHub Pages 開啟網頁，透過 MQTTGO 傳送音符與頻譜資料到 ESP32，並驅動 WS2812B 16x16 燈板產生即時燈光效果。

## 正式入口

- `index.html`：正式版主程式，已由最後音準修正版 `index_9_gestures_tuned_pitch.html` 更新而來。
- `manifest.webmanifest`：PWA 設定，可加入平板主畫面。
- `sw.js`：Service Worker 快取設定。
- `VERSION_SUCCESS.md`：v1 成功版的 MQTT、ESP32 與燈板設定紀錄。

## 手勢與音高

| 位置 | 手勢 | 簡譜 | 音名 |
| --- | --- | --- | --- |
| 1 | 食指單指 | 1 Do | C4 |
| 2 | 勝利 V | 2 Re | D4 |
| 3 | 三指手勢 | 3 Mi | E4 |
| 4 | 搖滾手勢 | 4 Fa | F4 |
| 5 | 張開手掌 | 5 Sol | G4 |
| 6 | 握拳 | 6 La | A4 |
| 7 | 捏合 | 7 Si | B4 |
| 8 | 拇指讚 | 1̇ 高音 Do | C5 |
| 9 | 小指手勢 | 2̇ 高音 Re | D5 |

## 平板使用建議

1. 將本專案部署到 GitHub Pages、Vercel 或 Netlify 等 HTTPS 網址。
2. 用 iPad Safari 或 Android Chrome 開啟網站。
3. 允許攝影機權限。
4. 建議橫向使用平板。
5. 測試九種手勢、音階準確度，以及同音重複演奏。

## 注意事項

- 不建議直接用 `file:///` 開啟，攝影機權限可能無法正常使用。
- 若更新後平板仍看到舊版本，請更新 `sw.js` 裡的 `CACHE_NAME`，或清除瀏覽器網站資料。
- v1 成功版使用 MQTTGO：網頁端 `wss://mqttgo.io:8084`，ESP32 端 `mqttgo.io:1883`。
- ESP32 程式可能包含 Wi-Fi 密碼，請只保存在本機私有備份，不要直接推到公開 GitHub。
