#!/bin/sh

# Function to check if the previous command was successful
check_status() {
  if [ $? -ne 0 ]; then
    echo "Error: Compilation failed. Exiting..."
    exit 1
  fi
}

# Build the project
echo "Building the project..."
start_time=$(date +%s)
# sudo make clean
sudo make -j$(nproc)
check_status
sudo make install
check_status
echo "Main build complete!"

echo "Building the targets"
cd targets/simple_switch
# sudo make clean
sudo make -j$(nproc)
check_status
sudo make install
check_status
echo "Simple switch build complete!"
cd ../simple_switch_grpc
# sudo make clean
sudo make -j$(nproc)
check_status
sudo make install
check_status
echo "Simple switch gRPC build complete!"
end_time=$(date +%s)

echo "[$(date)] Build time: $((end_time - start_time)) seconds"
echo "Done"
