const config = window.HANGOUT_DOWNLOAD || {};
const downloadButton = document.getElementById("downloadButton");
const versionText = document.getElementById("versionText");

const formatVersion = () => {
  const parts = [];
  if (config.version && config.version !== "local") {
    parts.push(`Version ${config.version}`);
  }
  if (config.fileSize) {
    parts.push(config.fileSize);
  }
  return parts.length > 0 ? parts.join(" · ") : "Latest installer";
};

if (config.installerUrl) {
  downloadButton.href = config.installerUrl;
}

versionText.textContent = formatVersion();
