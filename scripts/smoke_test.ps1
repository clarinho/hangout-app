param(
    [string]$BaseUrl = "http://127.0.0.1:8080",
    [string]$Username = "alice"
)

$login = Invoke-RestMethod `
    -Method Post `
    -Uri "$BaseUrl/api/v1/auth/login" `
    -ContentType "application/json" `
    -Body (@{ username = $Username } | ConvertTo-Json)

$token = $login.session.token
$headers = @{ Authorization = "Bearer $token" }

$servers = Invoke-RestMethod -Method Get -Uri "$BaseUrl/api/v1/servers" -Headers $headers
$serverId = $servers.servers[0].id

$channels = Invoke-RestMethod -Method Get -Uri "$BaseUrl/api/v1/servers/$serverId/channels" -Headers $headers
$channelId = $channels.channels[0].id

Invoke-RestMethod `
    -Method Post `
    -Uri "$BaseUrl/api/v1/channels/$channelId/messages" `
    -Headers $headers `
    -ContentType "application/json" `
    -Body (@{ content = "smoke test message" } | ConvertTo-Json) | Out-Null

$history = Invoke-RestMethod -Method Get -Uri "$BaseUrl/api/v1/channels/$channelId/messages?limit=10" -Headers $headers

Write-Host "Login user:" $login.user.username
Write-Host "Server count:" $servers.servers.Count
Write-Host "Channel count:" $channels.channels.Count
Write-Host "Recent message count:" $history.messages.Count
