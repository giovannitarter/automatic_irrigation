#!/bin/bash


PROJECT="automatic-irrigation"
IMAGE_NAME="${PROJECT}_builder"
VOLUME_NAME="${PROJECT}_build_cache"


WORKDIR="$(cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)"


BUILD_OPTS=""
if [ "$1" = "clean" ] || [ "$1" = "clear" ];
then
    BUILD_OPTS+="--no-cache"
    VOLUME_CLEAN="1"
fi


pushd "$WORKDIR"

pushd docker
docker build $BUILD_OPTS -t $IMAGE_NAME .
ERR_BUILD=$?
popd

if [ "$ERR_BUILD" != "0" ];
then
    echo "cannot build image"
    exit 1
fi


if [ -n "$VOLUME_CLEAN" ];
then
    docker volume rm $VOLUME_NAME
fi


if ! docker volume inspect $VOLUME_NAME;
then
    docker volume create $VOLUME_NAME
fi


if [ ! -e "VERSION" ];
then
    echo "0" > "VERSION"
fi
CVERSION="$(cat VERSION)"
CVERSION="$((CVERSION + 1))"

rm -rf "$WORKDIR/output"
mkdir -p "$WORKDIR/output"
docker run \
    --rm \
    -it \
    \
    --env-file "$WORKDIR/.env" \
    -e CVERSION="$CVERSION" \
    \
    -v"$WORKDIR/output":/output \
    -v"$WORKDIR/project":/root/project \
    \
    -v ${VOLUME_NAME}:/root/work \
    -e PLATFORMIO_WORKSPACE_DIR="/root/work/.pio" \
    -e PLATFORMIO_CORE_DIR="/root/work/core" \
    \
    $IMAGE_NAME


if [ -e "$WORKDIR/output/firmware.bin" ];
then

    echo "Version $CVERSION build successfully"

    echo "$CVERSION" > VERSION
#    mkdir -p "$WORKDIR/../mr_server/firmware/"
#    cp "$WORKDIR/output/manifest" "$WORKDIR/../mr_server/firmware/"
#    cp "$WORKDIR/output/firmware.bin" "$WORKDIR/../mr_server/firmware/"
#    
#    echo ""
#    echo "Manifest:"
#    cat "$WORKDIR/output/manifest"
#    echo ""
    
    rm -rf "$WORKDIR/output"

else
    echo "Error building firmware"
    echo "Not deploying"
fi
