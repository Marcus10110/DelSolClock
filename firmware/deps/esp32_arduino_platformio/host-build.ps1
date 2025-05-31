param(
    [switch]$Interactive
)

$ErrorActionPreference = "Stop"

# Set names
$ImageBaseName = "esp32-arduino-platformio-builder"
$ContainerName = "my-little-builder"
$DockerfilePath = "Dockerfile"

# Compute lowercase hash of Dockerfile
$DockerHash = (Get-FileHash -Path $DockerfilePath -Algorithm SHA256).Hash.ToLower()
$ImageName = "$ImageBaseName-$DockerHash"

# Check if image exists
$imageExists = docker images --format "{{.Repository}}" | Where-Object { $_ -eq $ImageName }

if (-not $imageExists) {
    Write-Host "Dockerfile changed or image doesn't exist. Rebuilding image: $ImageName..."
    
    # Wrap docker build in try/catch to stop on failure
    try {
        docker build -t $ImageName -f $DockerfilePath . --progress=plain
    } catch {
        Write-Error "Docker build failed. Exiting."
        exit 1
    }

    # 🧹 Remove older images with the same base name
    Write-Host "Cleaning up old images..."
    $oldImages = docker images --format "{{.Repository}} {{.ID}}" |
        Where-Object { $_ -like "$ImageBaseName-*" -and -not $_.StartsWith($ImageName) }

    foreach ($img in $oldImages) {
        $parts = $img -split " "
        $repo = $parts[0]
        $id = $parts[1]
        Write-Host "Removing old image: $repo ($id)"
        docker rmi $id
    }
} else {
    Write-Host "Reusing existing image: $ImageName"
}

if ($Interactive) {
    $containerExists = docker ps -a --format "{{.Names}}" | Where-Object { $_ -eq $ContainerName }

    if ($containerExists) {
        $containerImage = docker inspect -f '{{.Config.Image}}' $ContainerName
        if ($containerImage -ne $ImageName) {
            Write-Host "Container uses outdated image. Removing container: $ContainerName"
            docker rm -f $ContainerName
            $containerExists = $false
        }
    }

    if (-not $containerExists) {
        Write-Host "Creating new interactive container: $ContainerName"
        docker create -it --name $ContainerName $ImageName bash
    } else {
        Write-Host "Reusing existing container: $ContainerName"
    }

    Write-Host "Starting interactive shell..."
    docker start -ai $ContainerName
} else {
    # Ensure output directory exists
    $OutputDir = Join-Path -Path (Get-Location) -ChildPath "arduino-esp32"
    if (-not (Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir | Out-Null
    }

    Write-Host "Running guest-build.sh and logging output to build.log..."
    docker run --rm -it `
        -v "${OutputDir}:/arduino-esp32" `
        $ImageName bash -c "./guest-build.sh" | Tee-Object -FilePath "build.log"
}