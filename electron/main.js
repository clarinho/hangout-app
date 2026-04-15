const { app, BrowserWindow, clipboard, ipcMain } = require("electron");
const { autoUpdater } = require("electron-updater");
const path = require("path");
const packageConfig = require("../package.json");

const configuredApiBaseUrl =
  process.env.HANGOUT_API_BASE_URL ||
  packageConfig.hangout?.apiBaseUrl ||
  "http://127.0.0.1:8080";

const configuredUpdateFeedUrl =
  process.env.HANGOUT_UPDATE_FEED_URL ||
  packageConfig.hangout?.updateFeedUrl ||
  "";

const configureAutoUpdates = () => {
  autoUpdater.autoDownload = true;
  autoUpdater.autoInstallOnAppQuit = true;

  if (configuredUpdateFeedUrl) {
    autoUpdater.setFeedURL({
      provider: "generic",
      url: configuredUpdateFeedUrl
    });
  }

  autoUpdater.on("error", (error) => {
    console.warn("Auto-update check failed:", error.message);
  });

  if (app.isPackaged) {
    autoUpdater.checkForUpdatesAndNotify().catch((error) => {
      console.warn("Auto-update check failed:", error.message);
    });
  }
};

const createWindow = () => {
  const win = new BrowserWindow({
    width: 1320,
    height: 860,
    minWidth: 920,
    minHeight: 620,
    backgroundColor: "#07070b",
    title: "Hangout",
    titleBarStyle: "hiddenInset",
    autoHideMenuBar: true,
    show: false,
    webPreferences: {
      preload: path.join(__dirname, "preload.js"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  });

  win.once("ready-to-show", () => win.show());
  win.loadFile(path.join(__dirname, "index.html"));
};

app.whenReady().then(() => {
  ipcMain.handle("config:get", () => ({
    apiBaseUrl: configuredApiBaseUrl,
    updateFeedUrl: configuredUpdateFeedUrl,
    appVersion: app.getVersion(),
    isPackaged: app.isPackaged
  }));

  ipcMain.handle("clipboard:writeText", (_event, text) => {
    clipboard.writeText(String(text || ""));
    return true;
  });

  createWindow();
  configureAutoUpdates();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});
