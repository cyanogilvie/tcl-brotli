#include "brotliInt.h"

//#define STRINGIFY(x) #x

// Internal API {{{
//}}}
// Stubs API {{{
//}}}

static int compress_cmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) //{{{
{
	int					retcode = TCL_OK;
	int					i, index;
	static const char*	option_str[] = {
		"-quality",
		"-mode",
		"-window",
		"-largewindow",
		NULL
	};
	enum {
		TBROTLI_COMPRESS_QUALITY,
		TBROTLI_COMPRESS_MODE,
		TBROTLI_COMPRESS_WINDOW,
		TBROTLI_COMPRESS_LARGEWINDOW
	};
	const unsigned char*		bytes;
	int				byteslen;
	static const char*	mode_str[] = {
		"generic",
		"text",
		"font",
		NULL
	};
	int mode_map[] = {
		BROTLI_MODE_GENERIC,
		BROTLI_MODE_TEXT,
		BROTLI_MODE_FONT
	};
	enum BrotliEncoderMode	mode = BROTLI_DEFAULT_MODE;
	int						window = BROTLI_DEFAULT_WINDOW;
	int						quality = BROTLI_DEFAULT_QUALITY;
	int						allow_large_window = 0;
	uint8_t*				resbuf = NULL;
	size_t					max_output_len, encoded_size;

	if (objc < 2 || (objc-2) % 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "should be ?-option value? bytes");
		retcode = TCL_ERROR;
		goto done;
	}

	bytes = Tcl_GetByteArrayFromObj(objv[objc-1], &byteslen);

	for (i=1; i<objc-1; i++) {
		int opt;
		TEST_OK_LABEL(done, retcode, Tcl_GetIndexFromObj(interp, objv[i], option_str, "option", TCL_EXACT, &opt));
		switch (opt) {
			case TBROTLI_COMPRESS_QUALITY:
				TEST_OK_LABEL(done, retcode, Tcl_GetIntFromObj(interp, objv[++i], &quality));
				break;

			case TBROTLI_COMPRESS_MODE:
				TEST_OK_LABEL(done, retcode, Tcl_GetIndexFromObj(interp, objv[++i], mode_str, "mode", TCL_EXACT, &index));
				mode = mode_map[index];
				break;

			case TBROTLI_COMPRESS_WINDOW:
				TEST_OK_LABEL(done, retcode, Tcl_GetIntFromObj(interp, objv[++i], &window));
				break;

			case TBROTLI_COMPRESS_LARGEWINDOW:
				allow_large_window = 1;
				break;

			default:
				THROW_ERROR_LABEL(done, retcode, "Unhandled compress opt");
		}
	}
	if (quality < BROTLI_MIN_QUALITY || quality > BROTLI_MAX_QUALITY)
		THROW_ERROR_LABEL(done, retcode, "-quality must be in the range " STRINGIFY(BROTLI_MIN_QUALITY) " - " STRINGIFY(BROTLI_MAX_QUALITY));

	if (allow_large_window) {
		if (window < BROTLI_MIN_WINDOW_BITS || window > BROTLI_LARGE_MAX_WINDOW_BITS)
			THROW_ERROR_LABEL(done, retcode, "-window must be in the range " STRINGIFY(BROTLI_MIN_WINDOW_BITS) " - " STRINGIFY(BROTLI_LARGE_MAX_WINDOW_BITS));
	} else {
		if (window < BROTLI_MIN_WINDOW_BITS || window > BROTLI_MAX_WINDOW_BITS)
			THROW_ERROR_LABEL(done, retcode, "-window must be in the range " STRINGIFY(BROTLI_MIN_WINDOW_BITS) " - " STRINGIFY(BROTLI_MAX_WINDOW_BITS));
	}

	if (quality >= 2) {
		max_output_len = BrotliEncoderMaxCompressedSize(byteslen);
		if (max_output_len == 0)
			THROW_ERROR_LABEL(done, retcode, "Can't calculate output buffer size, too big?");
		resbuf = ckalloc(encoded_size = max_output_len);
	} else {
		resbuf = ckalloc(encoded_size = byteslen + 8192);	// TODO: figure out what this should be
	}

	if (BROTLI_FALSE == BrotliEncoderCompress(quality, window, mode, byteslen, (uint8_t*)bytes, &encoded_size, resbuf))
		THROW_ERROR_LABEL(done, retcode, "Error compressing data");
	if (encoded_size == 0)
		THROW_ERROR_LABEL(done, retcode, "Error compressing data");

	Tcl_SetObjResult(interp, Tcl_NewByteArrayObj((const unsigned char*)resbuf, encoded_size));

done:
	if (resbuf) {
		ckfree(resbuf);
		resbuf = NULL;
	}

	return retcode;
}

//}}}
void* custom_alloc(void* opaque, size_t size) //{{{
{
	ckalloc(size);
}

//}}}
void custom_free(void* opaque, void* address) //{{{
{
	ckfree(address);
}

//}}}
static int decompress_cmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) //{{{
{
	int						code = TCL_OK;
	const unsigned char*	enc_bytes = NULL;
	int						enc_len;
	int						sizehint = 0;
	BrotliDecoderState*		s = NULL;
	uint8_t*				out = NULL;
	size_t					available_out = 0, total_out = 0;
	size_t					available_in;
	const uint8_t*			next_in = NULL;
	uint8_t*				next_out = NULL;

	if (objc > 3)
		CHECK_ARGS(1, "bytes ?sizehint?");

	enc_bytes = Tcl_GetByteArrayFromObj(objv[1], &enc_len);

	if (objc >= 3)
		TEST_OK_LABEL(finally, code, Tcl_GetIntFromObj(interp, objv[2], &sizehint));

	if (sizehint == 0) {
		sizehint = 1u << 24;
	} else if (sizehint < 0) {
		THROW_ERROR_LABEL(finally, code, "sizehint may not be negative");
	}

	next_in = enc_bytes;
	available_in = enc_len;

	out = ckalloc(sizehint);
	next_out = out;
	available_out = sizehint;

	s = BrotliDecoderCreateInstance(custom_alloc, custom_free, NULL);

	for (;;) {
		BrotliDecoderResult	result = BrotliDecoderDecompressStream(s, &available_in, &next_in, &available_out, &next_out, &total_out);

		switch (result) {
			case BROTLI_DECODER_SUCCESS:
				goto success;

			case BROTLI_DECODER_RESULT_ERROR:
				THROW_ERROR_LABEL(finally, code, BrotliDecoderErrorString(BrotliDecoderGetErrorCode(s)));

			case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
				THROW_ERROR_LABEL(finally, code, "Input is truncated");

			case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
				{
					uint8_t*	new = NULL;

					sizehint *= 2;
					new = ckrealloc(out, sizehint);

					next_out = new + (next_out - out);
					available_out = sizehint - (next_out-new);
					out = new;
				}
				break;

			default:
				THROW_ERROR_LABEL(finally, code, "Unhandled result from BrotliDecoderDecompressStream");
		}
	}

success:
	Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(out, total_out));

finally:
	if (s) {
		BrotliDecoderDestroyInstance(s);
		s = NULL;
	}
	if (out) {
		ckfree(out);
		out = NULL;
	}

	return code;
}

//}}}

extern const BrotliStubs* const brotliConstStubsPtr;

#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT int Brotli_Init(Tcl_Interp* interp)
{
	int					code = TCL_OK;
	Tcl_Namespace*		ns = NULL;

#if USE_TCL_STUBS
	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL)
		return TCL_ERROR;
#endif

#define NS	"::brotli"

	ns = Tcl_CreateNamespace(interp, NS, NULL, NULL);
	TEST_OK(Tcl_Export(interp, ns, "*", 0));

	Tcl_CreateObjCommand(interp, NS "::compress",   compress_cmd,   NULL, NULL);
	Tcl_CreateObjCommand(interp, NS "::decompress", decompress_cmd, NULL, NULL);

	//fprintf(stderr, "Brotli_Init, providing package version (%s) with stubsPtr: %p\n", PACKAGE_VERSION, brotliConstStubsPtr);
	code = Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, brotliConstStubsPtr);
	if (code != TCL_OK) goto finally;

finally:
	return code;
}

#ifdef __cplusplus
}
#endif
