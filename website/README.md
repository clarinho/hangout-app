# Hangout Download Page

This folder is a static installer landing page for Hostinger.

Build the upload folder from the repo root:

```powershell
.\build-download-page
```

The script creates:

```text
website-dist/
```

Upload the **contents** of `website-dist` to Hostinger, either:

```text
public_html/
```

or a subfolder:

```text
public_html/download/
```

The generated page includes:

```text
index.html
styles.css
download.js
site-config.js
downloads/HangoutSetup.exe
```

The script copies the newest valid installer from `release/`, renames it to:

```text
downloads/HangoutSetup.exe
```

and writes the version into `site-config.js`.

Do not commit `website-dist`; it contains the large installer and is intentionally ignored.
