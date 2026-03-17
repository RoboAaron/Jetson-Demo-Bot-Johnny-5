#!/usr/bin/env bash
# Build Johnny-5 offline bundle for Jetson AGX Orin (arm64).
# Targets JetPack 6.x / Ubuntu 22.04 (Jammy) — ROS 2 Humble.
# By default runs incrementally: skips debs/wheels/clones/models that already exist.
# Force full re-download with: J5_BUNDLE_FRESH=1 ./build_j5_bundle.sh

set -u
set -o pipefail

BUNDLE_ROOT="${HOME}/j5_bundle"
LOG_FILE="${BUNDLE_ROOT}/bundle_build.log"
ERROR_FILE="${BUNDLE_ROOT}/ERRORS.txt"
FALLBACK_FILE="${BUNDLE_ROOT}/pip_wheels/FALLBACKS.txt"
HOST_ARCH="$(dpkg --print-architecture 2>/dev/null || uname -m)"

ROS2_PACKAGES=(
  ros-humble-desktop
  ros-humble-ros-base
  ros-humble-slam-toolbox
  ros-humble-nav2-bringup
  ros-humble-nav2-core
  ros-humble-nav2-msgs
  ros-humble-nav2-map-server
  ros-humble-robot-localization
  ros-humble-twist-mux
  ros-humble-teleop-twist-joy
  ros-humble-teleop-twist-keyboard
  ros-humble-rosbridge-suite
  ros-humble-tf2-tools
  ros-humble-tf2-ros
  ros-humble-xacro
  ros-humble-joint-state-publisher
  ros-humble-robot-state-publisher
  ros-humble-rviz2
  ros-humble-image-transport
  ros-humble-cv-bridge
  ros-humble-sensor-msgs
  ros-humble-geometry-msgs
  ros-humble-nav-msgs
  ros-humble-std-msgs
  python3-colcon-common-extensions
  python3-rosdep
  python3-vcstool
  python3-argcomplete
)

SYSTEM_PACKAGES=(
  libopencv-dev
  libudev-dev
  libusb-1.0-0-dev
  libbluetooth-dev
  libsoundio-dev
  portaudio19-dev
  python3-serial
  python3-numpy
  python3-scipy
  python3-smbus
  build-essential
  cmake
)

PYTHON_PACKAGES=(
  pyserial
  numpy
  scipy
  transforms3d
  smbus2
  websocket-client
  sounddevice
  pyaudio
  faster-whisper
  openwakeword
  depthai
  rclpy
)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p "${BUNDLE_ROOT}"
: > "${ERROR_FILE}"

exec > >(tee -a "${LOG_FILE}") 2>&1

log() {
  printf '\n[%s] %s\n' "$(date '+%F %T')" "$*"
}

pass() {
  printf 'PASS: %s\n' "$*"
}

warn() {
  printf 'WARN: %s\n' "$*"
}

fail() {
  printf 'FAIL: %s\n' "$*"
  printf '[%s] %s\n' "$(date '+%F %T')" "$*" >> "${ERROR_FILE}"
}

run_or_log() {
  local desc="$1"
  shift
  log "${desc}"
  if "$@"; then
    pass "${desc}"
    return 0
  fi
  fail "${desc}"
  return 1
}

safe_mkdirs() {
  mkdir -p \
    "${BUNDLE_ROOT}/debs/ros2" \
    "${BUNDLE_ROOT}/debs/system" \
    "${BUNDLE_ROOT}/pip_wheels" \
    "${BUNDLE_ROOT}/ros2_src/johnny5" \
    "${BUNDLE_ROOT}/ros2_src/ldlidar_stl_ros2" \
    "${BUNDLE_ROOT}/ros2_src/depthai-ros" \
    "${BUNDLE_ROOT}/models/whisper_cache" \
    "${BUNDLE_ROOT}/models/llm" \
    "${BUNDLE_ROOT}/models/llama_cpp" \
    "${BUNDLE_ROOT}/models/openwakeword" \
    "${BUNDLE_ROOT}/udev" \
    "${BUNDLE_ROOT}/scripts"
  : > "${FALLBACK_FILE}"
}

install_host_tools() {
  run_or_log \
    "Install host apt tools" \
    sudo apt-get update

  run_or_log \
    "Install required host packages" \
    sudo apt-get install -y \
      apt-rdepends dpkg-dev python3-pip git curl wget lz4 \
      ca-certificates gnupg lsb-release python3-venv

  run_or_log \
    "Install Python host packages" \
    python3 -m pip install --user --upgrade huggingface_hub openai-whisper openwakeword
}

configure_arm64_sources() {
  log "Configure arm64 apt sources for ROS 2 Humble and Ubuntu Jammy"

  if ! sudo curl -sSL \
    https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
    -o /usr/share/keyrings/ros-archive-keyring.gpg; then
    fail "Download ROS apt key"
  else
    pass "Download ROS apt key"
  fi

  cat <<'EOF' | sudo tee /etc/apt/sources.list.d/ros2-arm64.list >/dev/null
deb [arch=arm64 signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu jammy main
EOF

  cat <<'EOF' | sudo tee /etc/apt/sources.list.d/ubuntu-jammy-arm64.list >/dev/null
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-updates main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-security main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-backports main restricted universe multiverse
EOF

  if sudo dpkg --add-architecture arm64; then
    pass "Enable arm64 architecture"
  else
    warn "arm64 architecture may already be enabled"
  fi

  run_or_log \
    "Refresh apt metadata for jammy/arm64" \
    sudo apt-get update
}

reset_deb_dir() {
  local out_dir="$1"
  rm -f "${out_dir}"/*.deb "${out_dir}/Packages.gz"
}

# If J5_BUNDLE_FRESH=1, clear and re-download; otherwise skip existing artifacts (incremental).
deb_already_downloaded() {
  local out_dir="$1"
  local pkg="$2"
  # .deb filenames are like package_version_arch.deb
  test -n "$(find "${out_dir}" -maxdepth 1 -name "${pkg}_*.deb" 2>/dev/null)"
}

package_has_candidate() {
  local pkg="$1"
  local candidate
  candidate="$(apt-cache policy "${pkg}" 2>/dev/null | awk '/Candidate:/ {print $2; exit}')"
  [[ -n "${candidate}" && "${candidate}" != "(none)" ]]
}

download_one_deb_host() {
  local out_dir="$1"
  local pkg="$2"
  local arch="${3:-arm64}"

  if ! package_has_candidate "${pkg}:${arch}"; then
    return 1
  fi

  (
    cd "${out_dir}" || exit 1
    apt-get download "${pkg}:${arch}"
  ) && return 0
  return 1
}

download_dep_tree_host() {
  local out_dir="$1"
  local label="$2"
  shift 2
  local packages=("$@")
  local pkg dep
  local had_failure=0

  [[ -n "${J5_BUNDLE_FRESH:-}" ]] && reset_deb_dir "${out_dir}"
  mkdir -p "${out_dir}"

  for pkg in "${packages[@]}"; do
    log "Resolve dependencies for ${label}: ${pkg}"
    if ! deb_already_downloaded "${out_dir}" "${pkg}"; then
      if package_has_candidate "${pkg}:arm64"; then
        if download_one_deb_host "${out_dir}" "${pkg}" arm64; then
          pass "Downloaded ${pkg} to ${label}"
        else
          fail "Download ${pkg} to ${label}"
          had_failure=1
        fi
      fi
    fi
    local deps
    if ! deps="$(
      apt-rdepends "${pkg}" 2>/dev/null | awk '/^[^ ]/ {print $1}' | sort -u
    )"; then
      fail "Resolve dependencies for ${pkg}"
      had_failure=1
      continue
    fi

    while IFS= read -r dep; do
      [[ -z "${dep}" ]] && continue

      if deb_already_downloaded "${out_dir}" "${dep}"; then
        continue
      fi

      if ! package_has_candidate "${dep}:arm64"; then
        warn "Skipping non-downloadable arm64 dependency ${dep} for ${label}"
        continue
      fi

      if download_one_deb_host "${out_dir}" "${dep}" arm64; then
        pass "Downloaded ${dep} to ${label}"
      else
        fail "Download ${dep} to ${label}"
        had_failure=1
      fi
    done <<< "${deps}"
  done

  (
    cd "${out_dir}" || exit 1
    dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz
  ) && pass "Indexed ${label} local apt repo" || fail "Index ${label} local apt repo"

  return "${had_failure}"
}

download_dep_tree_docker() {
  local out_dir="$1"
  local label="$2"
  shift 2
  local packages=("$@")
  local list_file
  local had_failure=0

  if ! command -v docker >/dev/null 2>&1; then
    fail "Docker is required on amd64 hosts for clean jammy/arm64 deb resolution"
    warn "Run inside arm64 Docker: docker run --platform linux/arm64 -it --rm -v ~/j5_bundle:/bundle ubuntu:22.04 bash"
    return 1
  fi

  [[ -n "${J5_BUNDLE_FRESH:-}" ]] && reset_deb_dir "${out_dir}"
  mkdir -p "${out_dir}"
  list_file="${BUNDLE_ROOT}/.$(basename "${out_dir}")_packages.txt"
  printf '%s\n' "${packages[@]}" > "${list_file}"

  if ! docker run --platform linux/arm64 --rm \
    -v "${BUNDLE_ROOT}:/bundle" \
    ubuntu:22.04 \
    bash -lc '
      set -u
      set -o pipefail
      export DEBIAN_FRONTEND=noninteractive

      echo "deb http://ports.ubuntu.com/ubuntu-ports jammy main restricted universe multiverse" > /etc/apt/sources.list
      echo "deb http://ports.ubuntu.com/ubuntu-ports jammy-updates main restricted universe multiverse" >> /etc/apt/sources.list
      echo "deb http://ports.ubuntu.com/ubuntu-ports jammy-security main restricted universe multiverse" >> /etc/apt/sources.list
      rm -f /etc/apt/sources.list.d/*.list

      apt-get update
      apt-get install -y apt-rdepends dpkg-dev curl ca-certificates gnupg

      curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
        -o /usr/share/keyrings/ros-archive-keyring.gpg

      echo "deb [signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu jammy main" > /etc/apt/sources.list.d/ros2.list

      apt-get update 2>&1 | tee /bundle/apt_update.log

      echo "dpkg arch: $(dpkg --print-architecture)" >> /bundle/ros2_apt_policy.txt
      apt-cache policy ros-humble-desktop >> /bundle/ros2_apt_policy.txt 2>&1 || true

      package_has_candidate() {
        local pkg="$1"
        local candidate
        candidate="$(apt-cache policy "${pkg}" 2>/dev/null | awk '"'"'/Candidate:/ {print $2; exit}'"'"')"
        [[ -n "${candidate}" && "${candidate}" != "(none)" ]]
      }

      out_dir="/bundle/debs/'"$(basename "${out_dir}")"'"
      label="'"${label}"'"
      chmod 777 "${out_dir}" 2>/dev/null || true

      while IFS= read -r root_pkg; do
        root_pkg="${root_pkg%%[[:space:]]*}"
        [[ -z "${root_pkg}" ]] && continue
        echo "Resolving ${label}: ${root_pkg}"
        if ! ls "${out_dir}/${root_pkg}"_*.deb 1>/dev/null 2>&1; then
          if package_has_candidate "${root_pkg}"; then
            echo "Downloading root package: ${root_pkg}"
            ( cd "${out_dir}" || exit 1; apt-get download "${root_pkg}" ) || { echo "DOWNLOAD_FAIL ${root_pkg}" >> /bundle/ERRORS.txt; exit 1; }
          else
            echo "Skipping root package ${root_pkg} (no candidate in apt cache)" >> /bundle/ERRORS.txt
          fi
        fi
        deps="$(apt-rdepends "${root_pkg}" 2>/dev/null | awk '"'"'/^[^ ]/ {print $1}'"'"' | sort -u)"
        while IFS= read -r dep; do
          [[ -z "${dep}" ]] && continue
          if ls "${out_dir}/${dep}"_*.deb 1>/dev/null 2>&1; then continue; fi
          if ! package_has_candidate "${dep}"; then
            echo "SKIP ${dep}" >> /bundle/ERRORS.txt
            continue
          fi
          (
            cd "${out_dir}" || exit 1
            apt-get download "${dep}"
          ) || {
            echo "DOWNLOAD_FAIL ${dep}" >> /bundle/ERRORS.txt
            exit 1
          }
        done <<< "${deps}"
      done < "/bundle/'"$(basename "${list_file}")"'"

      (
        cd "${out_dir}" || exit 1
        dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz
      )
    '; then
    had_failure=1
    fail "Download ${label} via arm64 Docker"
  else
    pass "Downloaded ${label} via arm64 Docker"
  fi

  rm -f "${list_file}"
  return "${had_failure}"
}

# Host-side fallback: fetch ROS 2 arm64 Packages from repo, parse it, curl each ros-humble-* .deb.
# Use when Docker step leaves deps but apt cache has no ros-humble candidates.
download_ros2_humble_via_curl() {
  local out_dir="$1"
  shift
  local packages=("$@")
  local url_base="http://packages.ros.org/ros2/ubuntu"
  local tmp_packages="${BUNDLE_ROOT}/.tmp_ros2_Packages"
  local pkg fn

  log "Downloading ROS 2 jammy/arm64 Packages index (host curl)"
  if curl -sSL "${url_base}/dists/jammy/main/binary-arm64/Packages.gz" -o "${tmp_packages}.gz" && gzip -dc "${tmp_packages}.gz" > "${tmp_packages}" 2>/dev/null; then
    rm -f "${tmp_packages}.gz"
  else
    fail "Download/decompress ROS 2 Packages from jammy/arm64"
    rm -f "${tmp_packages}" "${tmp_packages}.gz"
    return 1
  fi
  if ! grep -q '^Package: ' "${tmp_packages}" 2>/dev/null; then
    head -80 "${tmp_packages}" > "${BUNDLE_ROOT}/.ros2_Packages_head.txt" 2>/dev/null || true
    fail "Downloaded ROS 2 index does not look like a Debian Packages file (no 'Package:' lines). Inspect ~/j5_bundle/.ros2_Packages_head.txt."
    rm -f "${tmp_packages}"
    return 1
  fi
  if ! grep -q '^Package: ros-humble-' "${tmp_packages}" 2>/dev/null; then
    rm -f "${tmp_packages}"
    fail "No ros-humble-* packages found in jammy/arm64 Packages index. Check network or packages.ros.org status."
    return 1
  fi
  head -80 "${tmp_packages}" > "${BUNDLE_ROOT}/.ros2_Packages_head.txt" 2>/dev/null || true

  log "Downloading ros-humble-* .deb files via host curl"
  for pkg in "${packages[@]}"; do
    [[ -z "${pkg}" ]] && continue
    if ls "${out_dir}/${pkg}"_*.deb 1>/dev/null 2>&1; then
      continue
    fi
    # Match paragraph: Package: <name>; then Filename: <path>. Reset found on blank line.
    fn="$(awk -v pkg="${pkg}" '
      /^$/ { found = 0; next }
      /^Package: / { found = ($2 == pkg); next }
      found && /^Filename: / { print $2; exit }
    ' "${tmp_packages}" 2>/dev/null)"
    if [[ -z "${fn}" ]]; then
      warn "No Filename for ${pkg} in Packages"
      continue
    fi
    if curl -sSL "${url_base}/${fn}" -o "${out_dir}/$(basename "${fn}")"; then
      pass "Downloaded ${pkg} via curl"
    else
      warn "curl failed for ${pkg}"
    fi
  done
  rm -f "${tmp_packages}"
  # out_dir may be root-owned from Docker; write to temp then sudo mv so we can reindex
  local tmp_gz
  tmp_gz="$(mktemp -p "${BUNDLE_ROOT}" .tmp_Packages.gz.XXXXXX 2>/dev/null)" || tmp_gz="${BUNDLE_ROOT}/.tmp_Packages.gz.$$"
  ( cd "${out_dir}" && dpkg-scanpackages . /dev/null | gzip -9c > "${tmp_gz}" ) && { sudo mv "${tmp_gz}" "${out_dir}/Packages.gz" 2>/dev/null || mv "${tmp_gz}" "${out_dir}/Packages.gz" 2>/dev/null; } && pass "Reindexed debs/ros2" || true
  rm -f "${tmp_gz}"
  return 0
}

download_dep_tree() {
  local out_dir="$1"
  local label="$2"
  shift 2
  local packages=("$@")
  local status=0

  if [[ "${HOST_ARCH}" = "amd64" ]]; then
    log "Using arm64 Docker for ${label} on amd64 host"
    download_dep_tree_docker "${out_dir}" "${label}" "${packages[@]}" || status=$?
  else
    download_dep_tree_host "${out_dir}" "${label}" "${packages[@]}" || status=$?
  fi

  if [[ "${status}" -ne 0 && "${HOST_ARCH}" = "amd64" ]]; then
    warn "Run inside arm64 Docker: docker run --platform linux/arm64 -it --rm -v ~/j5_bundle:/bundle ubuntu:22.04 bash"
  fi

  return "${status}"
}

pip_artifact_exists() {
  local pkg="$1"
  local base="${pkg//-/_}"
  find "${BUNDLE_ROOT}/pip_wheels" -maxdepth 1 \( -name "${base}"*.whl -o -name "${base}"*.tar.gz \) -print -quit 2>/dev/null | grep -q .
}

download_python_artifacts() {
  local pkg
  mkdir -p "${BUNDLE_ROOT}/pip_wheels"

  for pkg in "${PYTHON_PACKAGES[@]}"; do
    if [[ -z "${J5_BUNDLE_FRESH:-}" ]] && pip_artifact_exists "${pkg}"; then
      pass "Skipping ${pkg} (already in pip_wheels)"
      continue
    fi
    log "Download aarch64 wheel for ${pkg}"
    if python3 -m pip download \
      --platform manylinux2014_aarch64 \
      --platform manylinux_2_17_aarch64 \
      --platform manylinux_2_28_aarch64 \
      --platform linux_aarch64 \
      --implementation cp \
      --python-version 310 \
      --abi cp310 \
      --only-binary=:all: \
      --no-deps \
      -d "${BUNDLE_ROOT}/pip_wheels" \
      "${pkg}"; then
      pass "Downloaded binary wheel for ${pkg}"
      continue
    fi

    warn "Falling back to source distribution for ${pkg}"
    echo "${pkg}" >> "${FALLBACK_FILE}"
    if python3 -m pip download \
      --platform manylinux2014_aarch64 \
      --platform manylinux_2_17_aarch64 \
      --platform manylinux_2_28_aarch64 \
      --platform linux_aarch64 \
      --implementation cp \
      --python-version 310 \
      --abi cp310 \
      --no-deps \
      -d "${BUNDLE_ROOT}/pip_wheels" \
      "${pkg}"; then
      pass "Downloaded source fallback for ${pkg}"
    else
      fail "Download Python artifact for ${pkg}"
    fi
  done
}

clone_and_pack_repo() {
  local url="$1"
  local name="$2"
  local dest="${BUNDLE_ROOT}/ros2_src/${name}"
  local parent
  local branch
  parent="$(dirname "${dest}")"
  local tarball="${parent}/${name}.tar.gz"

  if [[ -z "${J5_BUNDLE_FRESH:-}" && -f "${tarball}" ]]; then
    pass "Skipping ${name} (${tarball} exists)"
    return 0
  fi

  if git ls-remote "${url}" >/dev/null 2>&1; then
    pass "Verified connectivity to ${url}"
  else
    fail "Verify connectivity to ${url}"
    return 1
  fi

  rm -rf "${dest}" "${parent}/${name}.tar.gz"

  branch="$(git ls-remote --symref "${url}" HEAD 2>/dev/null | awk '/^ref:/ {sub("refs/heads/","",$2); print $2; exit}')"
  [[ -n "${branch}" ]] || branch="main"

  if timeout 120 git clone \
    --depth 1 \
    --single-branch \
    --branch "${branch}" \
    --filter=blob:none \
    --progress \
    "${url}" "${dest}"; then
    pass "Cloned ${name}"
  else
    warn "Timed out cloning ${name}; falling back to GitHub source tarball"
    if ! fetch_github_tarball "${url}" "${branch}" "${dest}" "${name}"; then
      fail "Clone ${name}"
      return 1
    fi
  fi

  (
    cd "${parent}" || exit 1
    tar czf "${name}.tar.gz" "${name}"
  ) && pass "Packed ${name}.tar.gz" || fail "Pack ${name}.tar.gz"
}

fetch_github_tarball() {
  local url="$1"
  local branch="$2"
  local dest="$3"
  local name="$4"
  local repo_path
  local tarball_url
  local tmp_tar
  local extracted_dir
  local parent

  repo_path="${url#https://github.com/}"
  repo_path="${repo_path%.git}"
  tarball_url="https://codeload.github.com/${repo_path}/tar.gz/refs/heads/${branch}"
  parent="$(dirname "${dest}")"
  tmp_tar="$(mktemp "/tmp/${name}.XXXXXX.tar.gz")"

  if ! curl -L --fail --progress-bar "${tarball_url}" -o "${tmp_tar}"; then
    rm -f "${tmp_tar}"
    return 1
  fi

  extracted_dir="${parent}/${name}-${branch}"
  rm -rf "${dest}" "${extracted_dir}"

  if ! tar xzf "${tmp_tar}" -C "${parent}"; then
    rm -f "${tmp_tar}"
    return 1
  fi

  rm -f "${tmp_tar}"

  if [[ ! -d "${extracted_dir}" ]]; then
    return 1
  fi

  mv "${extracted_dir}" "${dest}" && pass "Downloaded ${name} tarball fallback"
}

clone_and_pack_depthai() {
  clone_and_pack_repo "https://github.com/luxonis/depthai-ros.git" "depthai-ros"
}

download_whisper_model() {
  if [[ -z "${J5_BUNDLE_FRESH:-}" ]] && [[ -d "${BUNDLE_ROOT}/models/whisper_cache" ]] && [[ -n "$(ls -A "${BUNDLE_ROOT}/models/whisper_cache" 2>/dev/null)" ]]; then
    pass "Skipping Whisper (whisper_cache already present)"
    return 0
  fi
  python3 - <<'PY'
import os
import shutil
import whisper

bundle = os.path.expanduser('~/j5_bundle/models/whisper_cache')
src = os.path.expanduser('~/.cache/whisper')

whisper.load_model('small.en')

if os.path.exists(bundle):
    shutil.rmtree(bundle)
shutil.copytree(src, bundle)
print(f"Copied whisper cache to {bundle}")
PY
}

download_llm_model() {
  local gguf="${BUNDLE_ROOT}/models/llm/Llama-3.2-3B-Instruct-Q4_K_M.gguf"
  if [[ -z "${J5_BUNDLE_FRESH:-}" && -f "${gguf}" ]]; then
    pass "Skipping Llama GGUF (already present)"
    return 0
  fi
  python3 - <<'PY'
import os
from huggingface_hub import hf_hub_download

target = os.path.expanduser('~/j5_bundle/models/llm')
os.makedirs(target, exist_ok=True)
hf_hub_download(
    repo_id='bartowski/Llama-3.2-3B-Instruct-GGUF',
    filename='Llama-3.2-3B-Instruct-Q4_K_M.gguf',
    local_dir=target,
)
print(f"Downloaded GGUF into {target}")
PY
}

download_openwakeword_models() {
  if [[ -z "${J5_BUNDLE_FRESH:-}" ]] && [[ -d "${BUNDLE_ROOT}/models/openwakeword" ]] && [[ -n "$(ls -A "${BUNDLE_ROOT}/models/openwakeword" 2>/dev/null)" ]]; then
    pass "Skipping openWakeWord (models already present)"
    return 0
  fi
  python3 - <<'PY'
import os
import shutil
import site
import subprocess
import sys

bundle = os.path.expanduser('~/j5_bundle/models/openwakeword')
venv_dir = os.path.expanduser('~/j5_bundle/.tmp_openwakeword_venv')

if os.path.exists(venv_dir):
    shutil.rmtree(venv_dir)

subprocess.run([sys.executable, '-m', 'venv', venv_dir], check=True)
venv_python = os.path.join(venv_dir, 'bin', 'python')
venv_pip = os.path.join(venv_dir, 'bin', 'pip')

subprocess.run([
    venv_pip, 'install', '-q',
    'numpy<2', 'onnxruntime', 'tqdm', 'requests',
    'scipy', 'scikit-learn', 'pandas', 'numexpr', 'bottleneck'
], check=True)
subprocess.run([venv_pip, 'install', '-q', 'openwakeword==0.6.0', '--no-deps'], check=True)

subprocess.run([
    venv_python, '-c',
    'import openwakeword; openwakeword.utils.download_models()'
], check=True)

site_packages = subprocess.check_output([
    venv_python, '-c', 'import site; print(site.getsitepackages()[0])'
], text=True).strip()
src = os.path.join(site_packages, 'openwakeword', 'resources', 'models')

if os.path.exists(bundle):
    shutil.rmtree(bundle)
shutil.copytree(src, bundle)
shutil.rmtree(venv_dir)
print(f"Copied openWakeWord models to {bundle}")
PY
}

download_llama_cpp_release() {
  if [[ -z "${J5_BUNDLE_FRESH:-}" ]] && [[ -d "${BUNDLE_ROOT}/models/llama_cpp" ]] && [[ -n "$(ls -A "${BUNDLE_ROOT}/models/llama_cpp" 2>/dev/null)" ]]; then
    pass "Skipping llama.cpp (already present)"
    return 0
  fi
  python3 - <<'PY'
import json
import os
import shutil
import tempfile
import tarfile
import urllib.request
import zipfile

api = 'https://api.github.com/repos/ggerganov/llama.cpp/releases/latest'
target = os.path.expanduser('~/j5_bundle/models/llama_cpp')
os.makedirs(target, exist_ok=True)

with urllib.request.urlopen(api) as resp:
    release = json.load(resp)

asset = None
for candidate in release.get('assets', []):
    name = candidate.get('name', '').lower()
    if 'ubuntu' in name and ('aarch64' in name or 'arm64' in name) and name.endswith('.zip'):
        asset = candidate
        break

if not asset:
    for candidate in release.get('assets', []):
        name = candidate.get('name', '').lower()
        if ('aarch64' in name or 'arm64' in name) and ('linux' in name or 'openeuler' in name):
            asset = candidate
            break

if not asset:
    raise RuntimeError('No llama.cpp Linux aarch64 asset found in latest release')

suffix = '.zip' if asset['name'].endswith('.zip') else '.tar.gz'
tmp = tempfile.NamedTemporaryFile(delete=False, suffix=suffix)
tmp.close()
urllib.request.urlretrieve(asset['browser_download_url'], tmp.name)

for child in os.listdir(target):
    path = os.path.join(target, child)
    if os.path.isdir(path):
        shutil.rmtree(path)
    else:
        os.remove(path)

if asset['name'].endswith('.zip'):
    with zipfile.ZipFile(tmp.name) as zf:
        zf.extractall(target)
else:
    with tarfile.open(tmp.name, 'r:*') as tf:
        tf.extractall(target)

os.remove(tmp.name)
print(f"Downloaded and extracted {asset['name']} to {target}")
PY
}

write_udev_rules() {
  cat > "${BUNDLE_ROOT}/udev/99-ldlidar.rules" <<'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="ldlidar", MODE="0666"
EOF

  cat > "${BUNDLE_ROOT}/udev/80-movidius.rules" <<'EOF'
SUBSYSTEM=="usb", ATTRS{idVendor}=="03e7", MODE="0666"
EOF

  cat > "${BUNDLE_ROOT}/udev/49-teensy.rules" <<'EOF'
SUBSYSTEM=="usb", ATTRS{idVendor}=="16c0", MODE="0666", GROUP="plugdev"
EOF

  cat > "${BUNDLE_ROOT}/udev/50-esp32.rules" <<'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", SYMLINK+="esp32_%n", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", SYMLINK+="esp32_%n", MODE="0666"
EOF

  pass "Wrote udev rules"
}

write_jetson_scripts() {
  cat > "${BUNDLE_ROOT}/scripts/01_configure_offline_apt.sh" <<'EOF'
#!/usr/bin/env bash
set -u
set -o pipefail
BUNDLE_ROOT="${1:-/mnt/j5bundle}"
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

[[ -d "${BUNDLE_ROOT}/debs/ros2" && -d "${BUNDLE_ROOT}/debs/system" ]] || { fail "Bundle not found at ${BUNDLE_ROOT}"; exit 1; }

# Generate apt index (Packages.gz) if missing so "file:" repos work
for dir in "${BUNDLE_ROOT}/debs/ros2" "${BUNDLE_ROOT}/debs/system"; do
  if [[ ! -f "${dir}/Packages.gz" && ! -f "${dir}/Packages" ]]; then
    if command -v dpkg-scanpackages >/dev/null 2>&1; then
      ( cd "${dir}" && dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz ) && pass "Generated index $(basename "${dir}")" || fail "Generate index $(basename "${dir}")"
    else
      fail "Missing ${dir}/Packages.gz and dpkg-scanpackages not found. Install dpkg-dev or use a bundle built with index."
      exit 1
    fi
  fi
done

sudo mkdir -p /etc/apt/sources.list.d.disabled_j5
if [[ -f /etc/apt/sources.list ]]; then
  if [[ ! -f /etc/apt/sources.list.j5bundle.bak ]]; then
    sudo cp /etc/apt/sources.list /etc/apt/sources.list.j5bundle.bak && pass "Backed up /etc/apt/sources.list" || fail "Backup /etc/apt/sources.list"
  fi
  echo "# Disabled for offline bundle; original in sources.list.j5bundle.bak" | sudo tee /etc/apt/sources.list >/dev/null
  pass "Disabled online apt sources"
fi

for file in /etc/apt/sources.list.d/*.list; do
  [[ -e "${file}" ]] || continue
  [[ "${file}" == "/etc/apt/sources.list.d/j5-bundle.list" ]] && continue
  sudo mv "${file}" /etc/apt/sources.list.d.disabled_j5/ && pass "Disabled ${file}" || fail "Disable ${file}"
done

cat <<APT | sudo tee /etc/apt/sources.list.d/j5-bundle.list >/dev/null
deb [trusted=yes] file://${BUNDLE_ROOT}/debs/ros2 ./
deb [trusted=yes] file://${BUNDLE_ROOT}/debs/system ./
APT

sudo apt-get update && pass "Configured local apt sources" || fail "Configure local apt sources"
echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF

  cat > "${BUNDLE_ROOT}/scripts/02_install_ros2.sh" <<'EOF'
#!/usr/bin/env bash
set -u
set -o pipefail
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

packages=(
  ros-humble-desktop
  ros-humble-ros-base
  ros-humble-slam-toolbox
  ros-humble-nav2-bringup
  ros-humble-nav2-core
  ros-humble-nav2-msgs
  ros-humble-nav2-map-server
  ros-humble-robot-localization
  ros-humble-twist-mux
  ros-humble-teleop-twist-joy
  ros-humble-teleop-twist-keyboard
  ros-humble-rosbridge-suite
  ros-humble-tf2-tools
  ros-humble-tf2-ros
  ros-humble-xacro
  ros-humble-joint-state-publisher
  ros-humble-robot-state-publisher
  ros-humble-rviz2
  ros-humble-image-transport
  ros-humble-cv-bridge
  ros-humble-sensor-msgs
  ros-humble-geometry-msgs
  ros-humble-nav-msgs
  ros-humble-std-msgs
  python3-colcon-common-extensions
  python3-rosdep
  python3-vcstool
  python3-argcomplete
)

sudo apt-get install -y "${packages[@]}" && pass "Installed ROS 2 offline packages" || fail "Install ROS 2 offline packages"
echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF

  cat > "${BUNDLE_ROOT}/scripts/03_install_python_deps.sh" <<'EOF'
#!/usr/bin/env bash
set -u
set -o pipefail
BUNDLE_ROOT="${1:-/mnt/j5bundle}"
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

packages=(pyserial numpy scipy transforms3d smbus2 websocket-client sounddevice pyaudio faster-whisper openwakeword depthai)

for pkg in "${packages[@]}"; do
  python3 -m pip install --no-index --find-links="${BUNDLE_ROOT}/pip_wheels" "${pkg}" \
    && pass "Installed ${pkg}" \
    || fail "Install ${pkg}"
done

if ls "${BUNDLE_ROOT}"/pip_wheels/rclpy-* >/dev/null 2>&1; then
  python3 -m pip install --no-index --find-links="${BUNDLE_ROOT}/pip_wheels" rclpy \
    && pass "Installed rclpy" \
    || fail "Install rclpy"
else
  pass "Skipped rclpy pip install (provided by ROS 2 apt on Jetson)"
fi

echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF

  cat > "${BUNDLE_ROOT}/scripts/04_build_ros2_workspace.sh" <<'EOF'
#!/usr/bin/env bash
set -u
set -o pipefail
BUNDLE_ROOT="${1:-/mnt/j5bundle}"
WS="${HOME}/ros2_ws"
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

source /opt/ros/humble/setup.bash || { fail "Source /opt/ros/humble/setup.bash"; exit 1; }

mkdir -p "${WS}/src"
cd "${WS}/src" || exit 1

tar xzf "${BUNDLE_ROOT}/ros2_src/johnny5.tar.gz" && pass "Extracted johnny5.tar.gz" || fail "Extract johnny5.tar.gz"
tar xzf "${BUNDLE_ROOT}/ros2_src/ldlidar_stl_ros2.tar.gz" && pass "Extracted ldlidar_stl_ros2.tar.gz" || fail "Extract ldlidar_stl_ros2.tar.gz"
tar xzf "${BUNDLE_ROOT}/ros2_src/depthai-ros.tar.gz" && pass "Extracted depthai-ros.tar.gz" || fail "Extract depthai-ros.tar.gz"

touch "${WS}/src/depthai-ros/COLCON_IGNORE" && pass "Disabled depthai-ros default build (source preserved for later bring-up)" || fail "Disable depthai-ros default build"

ln -sfn "${WS}/src/johnny5/jetson/ros2" "${WS}/src/balance_bridge" && pass "Linked balance_bridge" || fail "Link balance_bridge"
ln -sfn "${WS}/src/johnny5/johnny5_bringup" "${WS}/src/johnny5_bringup" && pass "Linked johnny5_bringup" || fail "Link johnny5_bringup"
ln -sfn "${WS}/src/johnny5/johnny5_description" "${WS}/src/johnny5_description" && pass "Linked johnny5_description" || fail "Link johnny5_description"
ln -sfn "${WS}/src/johnny5/johnny5_sensor_fusion" "${WS}/src/johnny5_sensor_fusion" && pass "Linked johnny5_sensor_fusion" || fail "Link johnny5_sensor_fusion"

cd "${WS}" || exit 1
colcon build --packages-skip johnny5_gazebo --cmake-args -DCMAKE_BUILD_TYPE=Release \
  && pass "Built ROS 2 workspace" \
  || fail "Build ROS 2 workspace"

grep -qxF "source /opt/ros/humble/setup.bash" "${HOME}/.bashrc" || echo "source /opt/ros/humble/setup.bash" >> "${HOME}/.bashrc"
grep -qxF "source ${WS}/install/setup.bash" "${HOME}/.bashrc" || echo "source ${WS}/install/setup.bash" >> "${HOME}/.bashrc"
pass "Updated ~/.bashrc"

echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF

  cat > "${BUNDLE_ROOT}/scripts/05_install_udev_rules.sh" <<'EOF'
#!/usr/bin/env bash
set -u
set -o pipefail
BUNDLE_ROOT="${1:-/mnt/j5bundle}"
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

sudo cp "${BUNDLE_ROOT}"/udev/*.rules /etc/udev/rules.d/ \
  && pass "Copied udev rules" \
  || fail "Copy udev rules"

sudo udevadm control --reload-rules && sudo udevadm trigger \
  && pass "Reloaded udev rules" \
  || fail "Reload udev rules"

sudo usermod -aG dialout,plugdev "${USER}" \
  && pass "Added ${USER} to dialout,plugdev" \
  || fail "Add ${USER} to dialout,plugdev"

echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF

  cat > "${BUNDLE_ROOT}/scripts/06_copy_models.sh" <<'EOF'
#!/usr/bin/env bash
set -u
set -o pipefail
BUNDLE_ROOT="${1:-/mnt/j5bundle}"
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

mkdir -p "${HOME}/.cache/whisper" "${HOME}/models/llm" "${HOME}/models/llama_cpp" "${HOME}/.local/share/openwakeword"
cp -a "${BUNDLE_ROOT}/models/whisper_cache/." "${HOME}/.cache/whisper/" && pass "Copied Whisper cache" || fail "Copy Whisper cache"
cp -a "${BUNDLE_ROOT}/models/llm/." "${HOME}/models/llm/" && pass "Copied LLM model" || fail "Copy LLM model"
cp -a "${BUNDLE_ROOT}/models/llama_cpp/." "${HOME}/models/llama_cpp/" && pass "Copied llama.cpp binaries" || fail "Copy llama.cpp binaries"
cp -a "${BUNDLE_ROOT}/models/openwakeword/." "${HOME}/.local/share/openwakeword/" && pass "Copied openWakeWord models" || fail "Copy openWakeWord models"

echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF

  cat > "${BUNDLE_ROOT}/scripts/07_create_systemd_service.sh" <<'EOF'
#!/usr/bin/env bash
set -u
set -o pipefail
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

USER_NAME="${SUDO_USER:-${USER}}"
USER_HOME="$(getent passwd "${USER_NAME}" | cut -d: -f6)"
[[ -n "${USER_HOME}" ]] || USER_HOME="${HOME}"

sudo tee /etc/systemd/system/johnny5.service >/dev/null <<EOF_SERVICE
[Unit]
Description=Johnny-5 Balance Bridge
After=network.target

[Service]
Type=simple
User=${USER_NAME}
WorkingDirectory=${USER_HOME}
ExecStart=/bin/bash -lc "source /opt/ros/humble/setup.bash && source ${USER_HOME}/ros2_ws/install/setup.bash && ros2 launch balance_bridge balance_bridge.launch.py"
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF_SERVICE

sudo systemctl daemon-reload && pass "Reloaded systemd" || fail "Reload systemd"
sudo systemctl enable johnny5.service && pass "Enabled johnny5.service" || fail "Enable johnny5.service"

echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF

  cat > "${BUNDLE_ROOT}/scripts/08_verify_install.sh" <<'EOF'
#!/usr/bin/env bash
set +u
set -o pipefail
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

source /opt/ros/humble/setup.bash || { fail "Source ROS 2 environment"; exit 1; }
[[ -f "${HOME}/ros2_ws/install/setup.bash" ]] && source "${HOME}/ros2_ws/install/setup.bash"

ros2 doctor >/dev/null 2>&1 && pass "ros2 doctor" || fail "ros2 doctor"
PKGS=$(ros2 pkg list 2>/dev/null) || true
echo "$PKGS" | grep -q johnny && pass "ros2 pkg list | grep johnny" || fail "ros2 pkg list | grep johnny"
echo "$PKGS" | grep -q balance_bridge && pass "balance_bridge package visible" || fail "balance_bridge package visible"

lsusb | grep -q 10c4 && pass "LiDAR / CP210x visible in lsusb" || fail "LiDAR / CP210x visible in lsusb"
lsusb | grep -q 03e7 && pass "OAK-D / Myriad X visible in lsusb" || fail "OAK-D / Myriad X visible in lsusb"
lsusb | grep -q 16c0 && pass "Teensy visible in lsusb" || fail "Teensy visible in lsusb"

ros2 launch balance_bridge balance_bridge.launch.py >/tmp/johnny5_verify_bridge.log 2>&1 &
BRIDGE_PID=$!
sleep 8
TOPICS=$(ros2 topic list 2>/dev/null) || true
if echo "$TOPICS" | grep -Eq '^/odom$|^/robot_state$|^/imu/roll$'; then
  pass "Bridge topics visible"
else
  fail "Bridge topics visible"
fi
kill "${BRIDGE_PID}" >/dev/null 2>&1 || true
wait "${BRIDGE_PID}" 2>/dev/null || true

echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF

  chmod +x "${BUNDLE_ROOT}"/scripts/*.sh
  pass "Wrote Jetson-side install scripts"
}

write_readme() {
  cat > "${BUNDLE_ROOT}/README.txt" <<'EOF'
Johnny-5 Offline Bundle for Jetson AGX Orin
==========================================

This bundle targets JetPack 6.x on Jetson AGX Orin (Ubuntu 22.04 / arm64).

USB mount example on Jetson:
  sudo mkdir -p /mnt/j5bundle
  sudo mount /dev/sda1 /mnt/j5bundle

Run these scripts in order:
  01_configure_offline_apt.sh
  02_install_ros2.sh
  03_install_python_deps.sh
  04_build_ros2_workspace.sh
  05_install_udev_rules.sh
  06_copy_models.sh
  07_create_systemd_service.sh
  08_verify_install.sh

Expected total install time on Jetson:
  ~45 minutes

If a step fails:
  1. Check FALLBACKS.txt in pip_wheels/ for source-only Python packages.
  2. Re-run the individual numbered script that failed.
  3. Review bundle_build.log and ERRORS.txt from the host build.
  4. For optional depthai-ros source, note that it is bundled but skipped by default in workspace build.
EOF
  pass "Wrote README.txt"
}

print_summary() {
  summary_line() {
    local dir="$1"
    local label="$2"
    local size count
    size="$(du -sm "${dir}" 2>/dev/null | awk '{print $1}')"
    count="$(find "${dir}" -type f 2>/dev/null | wc -l | awk '{print $1}')"
    printf '%-18s: %s MB  (%s files)\n' "${label}" "${size:-0}" "${count:-0}"
  }

  local total_gb
  total_gb="$(du -sh "${BUNDLE_ROOT}" 2>/dev/null | awk '{print $1}')"

  echo
  echo "=== BUNDLE SUMMARY ==="
  summary_line "${BUNDLE_ROOT}/debs/ros2"   "debs/ros2/"
  summary_line "${BUNDLE_ROOT}/debs/system" "debs/system/"
  summary_line "${BUNDLE_ROOT}/pip_wheels"  "pip_wheels/"
  summary_line "${BUNDLE_ROOT}/ros2_src"    "ros2_src/"
  summary_line "${BUNDLE_ROOT}/models"      "models/"
  summary_line "${BUNDLE_ROOT}/udev"        "udev/"
  summary_line "${BUNDLE_ROOT}/scripts"     "scripts/"
  printf '%-18s: %s\n' "TOTAL" "${total_gb:-0}"
  echo
  cat <<'EOF'
sudo mkfs.ext4 /dev/sdX1
sudo mount /dev/sdX1 /mnt/usb
sudo cp -r ~/j5_bundle/* /mnt/usb/
sudo umount /mnt/usb
sync
EOF
}

main() {
  log "Build Johnny-5 offline bundle on internet-connected host"
  log "Incremental by default (skip existing). Full rebuild: J5_BUNDLE_FRESH=1. Typical full run: 45 min–2 h. Run verify_bundle.sh after build before copying to SD."
  safe_mkdirs

  install_host_tools
  if [[ "${HOST_ARCH}" != "amd64" ]]; then
    configure_arm64_sources
  else
    log "Skipping host arm64 apt source setup on amd64 host; deb download uses arm64 Docker"
  fi

  download_dep_tree "${BUNDLE_ROOT}/debs/ros2" "ROS 2 debs" "${ROS2_PACKAGES[@]}"

  # Fail fast: if ROS 2 step didn't pull actual ros-humble packages, try host-side curl fallback then re-check
  ROS2_HUMBLE_COUNT=$(find "${BUNDLE_ROOT}/debs/ros2" -maxdepth 1 -name "ros-humble-*.deb" 2>/dev/null | wc -l)
  if [[ "${ROS2_HUMBLE_COUNT}" -lt 20 ]]; then
    log "ROS 2 apt had no ros-humble candidates; trying host-side curl from packages.ros.org"
    download_ros2_humble_via_curl "${BUNDLE_ROOT}/debs/ros2" "${ROS2_PACKAGES[@]}" || true
    ROS2_HUMBLE_COUNT=$(find "${BUNDLE_ROOT}/debs/ros2" -maxdepth 1 -name "ros-humble-*.deb" 2>/dev/null | wc -l)
  fi
  if [[ "${ROS2_HUMBLE_COUNT}" -lt 20 ]]; then
    fail "ROS 2 debs: only ${ROS2_HUMBLE_COUNT} ros-humble-*.deb found (need >=20). Check Docker/arm64, network, or packages.ros.org status."
    exit 1
  fi
  pass "ROS 2 debs: ${ROS2_HUMBLE_COUNT} ros-humble packages (fail-fast check)"

  download_dep_tree "${BUNDLE_ROOT}/debs/system" "system debs" "${SYSTEM_PACKAGES[@]}"

  for dir in "${BUNDLE_ROOT}/debs/ros2" "${BUNDLE_ROOT}/debs/system"; do
    ( cd "${dir}" && dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz ) \
      && pass "Indexed $(basename "${dir}") for apt" || fail "Index $(basename "${dir}")"
  done

  download_python_artifacts

  clone_and_pack_repo "https://github.com/RoboAaron/Jetson-Demo-Bot-Johnny-5.git" "johnny5"
  clone_and_pack_repo "https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2.git" "ldlidar_stl_ros2"
  clone_and_pack_depthai

  run_or_log "Download Whisper small.en model" download_whisper_model
  run_or_log "Download Llama 3.2 3B GGUF" download_llm_model
  run_or_log "Download openWakeWord models" download_openwakeword_models
  run_or_log "Download llama.cpp aarch64 release" download_llama_cpp_release

  write_udev_rules
  write_jetson_scripts
  write_readme
  print_summary

  if [[ -x "${SCRIPT_DIR}/verify_bundle.sh" ]]; then
    "${SCRIPT_DIR}/verify_bundle.sh" "${BUNDLE_ROOT}" || true
  fi
}

main "$@"
