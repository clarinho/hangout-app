param(
    [Parameter(Mandatory = $true)]
    [string]$Droplet,

    [string]$RemoteUser = "root"
)

$target = "$RemoteUser@$Droplet"

ssh $target "mkdir -p /opt/hangout /var/lib/hangout /var/www/hangout-updates"
scp .\build\hangout_backend $target`:/opt/hangout/hangout_backend
scp .\deploy\hangout-backend.service $target`:/etc/systemd/system/hangout-backend.service
ssh $target "chmod +x /opt/hangout/hangout_backend && useradd --system --home /opt/hangout --shell /usr/sbin/nologin hangout 2>/dev/null || true && chown -R hangout:hangout /opt/hangout /var/lib/hangout && systemctl daemon-reload && systemctl enable --now hangout-backend && systemctl restart hangout-backend"
