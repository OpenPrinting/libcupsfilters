#!/bin/sh
# ci/install-visual-deps.sh
# ==========================
# Install dependencies needed for the visual output quality tests.
# Designed for Ubuntu 22.04 / 24.04 (GitHub Actions ubuntu-latest).
# All packages come from apt — no pip or venv required.
#
# Usage (from CI workflow):
#   sh ci/install-visual-deps.sh

set -ex

apt-get update -qq

DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ghostscript \
    poppler-utils \
    python3 \
    python3-numpy \
    python3-pil \
    python3-skimage \
    python3-scipy \
    cups-filters \
    cups

echo "Visual test dependencies installed successfully."

# Smoke-test the key tools
gs --version
python3 -c "from skimage.metrics import structural_similarity; print('scikit-image OK')"
python3 -c "from PIL import Image; print('Pillow OK')"
pdffonts -v 2>&1 | head -1 || true
