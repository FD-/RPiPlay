#!/bin/bash
rm -rf ./release
docker build . -t rpiplay_builder
mkdir release
docker run -it -v ${PWD}/release:/release rpiplay_builder /bin/bash -c "cp /work/RPiPlay/build/rpiplay /work/RPiPlay/build/lib* /release"
echo "RPiPlay has been built in the ./release folder. Execute it with \"cd release && ./rpiplay\"."
