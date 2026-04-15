param(
    [Parameter(Mandatory = $true)]
    [string]$Droplet,

    [string]$RemoteUser = "root"
)

$target = "$RemoteUser@$Droplet"

ssh $target "mkdir -p /var/www/hangout-updates"
scp .\release\*.yml $target`:/var/www/hangout-updates/
scp .\release\*.exe $target`:/var/www/hangout-updates/
scp .\release\*.blockmap $target`:/var/www/hangout-updates/
