$utf8bom = New-Object System.Text.UTF8Encoding($true)
$content = Get-Content "D:\code\project\roboticArm\my\HardWarePCB\MotorDriver\Motor-42.csv" -Encoding UTF8 -Raw
[System.IO.File]::WriteAllText("D:\code\project\roboticArm\my\HardWarePCB\MotorDriver\Motor-42.csv", $content, $utf8bom)
Write-Host "Done - BOM added"
