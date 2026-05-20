# compile.sh main.cpp out.js ModuleName

export SCRIPT_DIR=`dirname "$0"`

if [ ! -z "$EMSDK" ]
then
	export EMSDK_DIR="$EMSDK"
fi

if [ -z "$EMSDK_DIR" ]
then
	export EMSDK_DIR="${SCRIPT_DIR}/emsdk"
fi

if ! test -d "${EMSDK_DIR}"
then
	echo "SDK not found - cloning from Github"
	git clone https://github.com/emscripten-core/emsdk.git "${EMSDK_DIR}"
	pushd "${EMSDK_DIR}" && git pull && ./emsdk install latest && ./emsdk activate latest
	popd
fi
EMSDK_QUIET=1 . "${EMSDK_DIR}/emsdk_env.sh" \
	&& emcc --check \
	&& python3 --version \
	&& cmake --version

if [ "$#" -le 1 ]; then
	echo "Missing .cpp / .js arguments"
	exit 1
fi

INPUT_CPP="$1"
OUTPUT_JS="$2"
mkdir -p $(dirname $OUTPUT_JS)

MODULE_NAME="$3"
if [ -z "$MODULE_NAME" ]
then
	MODULE_NAME=$(basename "$OUTPUT_JS" ".${OUTPUT_JS##*.}")
fi

echo "$MODULE_NAME: $INPUT_CPP -> $OUTPUT_JS"

#	-sSTRICT -sASSERTIONS --closure=0 \

em++ \
	$INPUT_CPP -o "${OUTPUT_JS}" \
	-sEXPORT_NAME=$MODULE_NAME -DEXPORT_NAME=$MODULE_NAME \
	-I "${SCRIPT_DIR}" \
	-std=c++11 -O3 -ffast-math -fno-exceptions -fno-rtti \
    	--pre-js "${SCRIPT_DIR}/pre.js" --closure 0 \
	-Wall -Wextra -Wfatal-errors -Wpedantic -pedantic-errors \
	-sSINGLE_FILE=1 -sMODULARIZE -sENVIRONMENT=web,worker,shell -sNO_EXIT_RUNTIME=1  \
	-sFILESYSTEM=0 -sEXPORTED_RUNTIME_METHODS=HEAP8,UTF8ToString \
	-sINITIAL_MEMORY=512kb -sALLOW_MEMORY_GROWTH=1 -sMEMORY_GROWTH_GEOMETRIC_STEP=0.5 -sABORTING_MALLOC=1 \
	-sSTRICT=1 -sDYNAMIC_EXECUTION=0

# Remove UMD definition
node -e "let f=process.argv[1],fs=require('fs');fs.writeFileSync(f,fs.readFileSync(f,'utf8').split(\"if (typeof exports === 'object' && typeof module === 'object') {\")[0])" "${OUTPUT_JS}"
