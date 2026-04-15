const { app, BrowserWindow, clipboard, ipcMain } = require("electron");
const { autoUpdater } = require("electron-updater");
const path = require("path");
const packageConfig = require("../package.json");

const normalizeBaseUrl = (value, fallback = "", options = {}) => {
  const raw = String(value || fallback).trim();
  if (!raw) {
    return "";
  }

  const withProtocol = /^https?:\/\//i.test(raw) ? raw : `http://${raw}`;
  if (options.keepTrailingSlash) {
    return withProtocol;
  }

  return withProtocol.replace(/\/+$/, "");
};

const configuredApiBaseUrl = normalizeBaseUrl(
  process.env.HANGOUT_API_BASE_URL ||
  packageConfig.hangout?.apiBaseUrl ||
    "http://127.0.0.1:8080"
);

const configuredUpdateFeedUrl = normalizeBaseUrl(
  process.env.HANGOUT_UPDATE_FEED_URL ||
  packageConfig.hangout?.updateFeedUrl ||
    "",
  "",
  { keepTrailingSlash: true }
);

let updateState = {
  status: "idle",
  message: "Preparing update check.",
  percent: 0
};

const windows = new Set();

const publishUpdateState = (nextState) => {
  updateState = {
    ...updateState,
    ...nextState
  };

  for (const win of windows) {
    if (!win.isDestroyed()) {
      win.webContents.send("updates:status", updateState);
    }
  }
};

const configureAutoUpdates = () => {
  autoUpdater.autoDownload = true;
  autoUpdater.autoInstallOnAppQuit = false;

  if (configuredUpdateFeedUrl) {
    autoUpdater.setFeedURL({
      provider: "generic",
      url: configuredUpdateFeedUrl
    });
  }

  autoUpdater.on("checking-for-update", () => {
    publishUpdateState({
      status: "checking",
      message: "Checking for updates.",
      percent: 0
    });
  });

  autoUpdater.on("update-available", () => {
    publishUpdateState({
      status: "available",
      message: "Update found. Downloading now.",
      percent: 0
    });
  });

  autoUpdater.on("update-not-available", () => {
    publishUpdateState({
      status: "not-available",
      message: "App is up to date.",
      percent: 100
    });
  });

  autoUpdater.on("download-progress", (progress) => {
    publishUpdateState({
      status: "downloading",
      message: `Downloading update: ${Math.round(progress.percent)}%`,
      percent: Math.max(0, Math.min(100, progress.percent || 0))
    });
  });

  autoUpdater.on("update-downloaded", () => {
    publishUpdateState({
      status: "downloaded",
      message: "Update downloaded. Restarting to install.",
      percent: 100
    });

    setTimeout(() => {
      autoUpdater.quitAndInstall(true, true);
    }, 1200);
  });

  autoUpdater.on("error", (error) => {
    console.warn("Auto-update check failed:", error.message);
    publishUpdateState({
      status: "error",
      message: "Could not check for updates. Starting app.",
      error: error.message,
      percent: 0
    });
  });

  if (app.isPackaged) {
    autoUpdater.checkForUpdates().catch((error) => {
      console.warn("Auto-update check failed:", error.message);
      publishUpdateState({
        status: "error",
        message: "Could not check for updates. Starting app.",
        error: error.message,
        percent: 0
      });
    });
  } else {
    publishUpdateState({
      status: "skipped",
      message: "Update checks run in the installed app.",
      percent: 100
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

  windows.add(win);
  win.on("closed", () => windows.delete(win));
  win.webContents.once("did-finish-load", () => {
    win.webContents.send("updates:status", updateState);
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

  ipcMain.handle("updates:getState", () => updateState);

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
