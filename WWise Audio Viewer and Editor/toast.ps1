[void] [System.Reflection.Assembly]::LoadWithPartialName("System.Windows.Forms")

$objNotifyIcon = New-Object System.Windows.Forms.NotifyIcon

$objNotifyIcon.Icon = [System.Drawing.SystemIcons]::Information
$objNotifyIcon.BalloonTipIcon = "None" 
$objNotifyIcon.BalloonTipTitle = "Export Complete" 
$objNotifyIcon.BalloonTipText = "Your files have been converted and saved to a ZIP file"
$objNotifyIcon.Visible = $True

$objNotifyIcon.ShowBalloonTip(10000)