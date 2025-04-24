sudo apt-get update
sudo apt-get install -y coreutils make mtools xorriso

# Only clone if limine-bin directory doesn't exist
if [ ! -d "limine-bin" ]; then
  echo "Cloning limine bootloader..."
  git clone https://github.com/limine-bootloader/limine.git --branch=v9.2.3-binary --depth=1 limine-bin
else
  echo "limine-bin directory already exists, skipping clone"
fi

cd limine-bin
make
chmod +x limine
cd ..
bash makeiso.sh