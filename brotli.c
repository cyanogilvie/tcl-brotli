#include <brotliInt.h>

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

// Internal API <<<
//>>>
// Stubs API <<<
//>>>

static int compress_cmd(ClientData /*cdata*/, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) //<<<
{
	if (objc < 2 || (objc-2) % 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "should be ?-option value ...? bytes");
		return TCL_ERROR;
	}

	Tcl_Size				byteslen;
	const unsigned char*	bytes = Tcl_GetBytesFromObj(interp, objv[objc-1], &byteslen);
	if (!bytes) return TCL_ERROR;

	enum BrotliEncoderMode	mode = BROTLI_DEFAULT_MODE;
	int		window = BROTLI_DEFAULT_WINDOW;
	int		quality = BROTLI_DEFAULT_QUALITY;
	bool	allow_large_window = true;
	for (int i=1; i<objc-1; i++) {
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
		} opt;
		int optidx;
		TEST_OK(Tcl_GetIndexFromObj(interp, objv[i], option_str, "option", TCL_EXACT, &optidx));
		opt = optidx;
		switch (opt) {
			case TBROTLI_COMPRESS_QUALITY:
				TEST_OK(Tcl_GetIntFromObj(interp, objv[++i], &quality));
				break;

			case TBROTLI_COMPRESS_MODE:
				{
					static const char*	mode_str[] = {
						"generic",
						"text",
						"font",
						NULL
					};
					enum BrotliEncoderMode mode_map[] = {
						BROTLI_MODE_GENERIC,
						BROTLI_MODE_TEXT,
						BROTLI_MODE_FONT
					};
					int modeidx;
					TEST_OK(Tcl_GetIndexFromObj(interp, objv[++i], mode_str, "mode", TCL_EXACT, &modeidx));
					mode = mode_map[modeidx];
				}
				break;

			case TBROTLI_COMPRESS_WINDOW:
				TEST_OK(Tcl_GetIntFromObj(interp, objv[++i], &window));
				break;

			case TBROTLI_COMPRESS_LARGEWINDOW:
				allow_large_window = true;
				break;

			default:
				THROW_ERROR("Unhandled compress opt");
		}
	}
	if (quality < BROTLI_MIN_QUALITY || quality > BROTLI_MAX_QUALITY)
		THROW_ERROR("-quality must be in the range " STRINGIFY(BROTLI_MIN_QUALITY) " - " STRINGIFY(BROTLI_MAX_QUALITY));

	if (allow_large_window) {
		if (window < BROTLI_MIN_WINDOW_BITS || window > BROTLI_LARGE_MAX_WINDOW_BITS)
			THROW_ERROR("-window must be in the range " STRINGIFY(BROTLI_MIN_WINDOW_BITS) " - " STRINGIFY(BROTLI_LARGE_MAX_WINDOW_BITS));
	} else {
		if (window < BROTLI_MIN_WINDOW_BITS || window > BROTLI_MAX_WINDOW_BITS)
			THROW_ERROR("-window must be in the range " STRINGIFY(BROTLI_MIN_WINDOW_BITS) " - " STRINGIFY(BROTLI_MAX_WINDOW_BITS));
	}

	Tcl_Obj*	res = NULL;	defer { replace_tclobj(&res, NULL); }
	if (quality >= 2) {
		size_t	max_output_len = BrotliEncoderMaxCompressedSize(byteslen);
		if (max_output_len == 0) THROW_ERROR("Can't calculate output buffer size, too big?");
		replace_tclobj(&res, Tcl_NewByteArrayObj(NULL, max_output_len));
	} else {
		replace_tclobj(&res, Tcl_NewByteArrayObj(NULL, byteslen + 8192));
	}
	Tcl_Size	objsize;
	uint8_t*	resbuf = Tcl_GetBytesFromObj(interp, res, &objsize);
	if (!resbuf) return TCL_ERROR;
	size_t		encoded_size = objsize;

	if (BROTLI_FALSE == BrotliEncoderCompress(quality, window, mode, byteslen, (uint8_t*)bytes, &encoded_size, resbuf))
		THROW_ERROR("Error compressing data");
	if (encoded_size == 0)
		THROW_ERROR("Error compressing data");
	resbuf = Tcl_SetByteArrayLength(res, encoded_size);

	Tcl_SetObjResult(interp, res);

	return TCL_OK;
}

//>>>
void* custom_alloc(void* /*opaque*/, size_t size) //<<<
{
	return ckalloc(size);
}

//>>>
void custom_free(void* /*opaque*/, void* address) //<<<
{
	ckfree(address);
}

//>>>
static int decompress_cmd(ClientData /*cdata*/, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) //<<<
{
	enum {A_cmd, A_BYTES, A_args, A_SIZEHINT=A_args, A_objc};
	CHECK_RANGE_ARGS("bytes, ?sizehint?");

	Tcl_Size				enc_len;
	const unsigned char*	enc_bytes = Tcl_GetBytesFromObj(interp, objv[A_BYTES], &enc_len);
	if (!enc_bytes) return TCL_ERROR;

	int	sizehint = 0;
	if (objc > A_SIZEHINT) TEST_OK(Tcl_GetIntFromObj(interp, objv[A_SIZEHINT], &sizehint));
	if (sizehint == 0)		sizehint = 1u << 24;
	else if (sizehint < 0)	THROW_ERROR("sizehint may not be negative");

	const uint8_t*	next_in = enc_bytes;
	size_t			available_in = enc_len;

	uint8_t*	out = ckalloc(sizehint);	defer { ckfree(out); }
	uint8_t*	next_out = out;
	size_t		available_out = sizehint;

	BrotliDecoderState* s = BrotliDecoderCreateInstance(custom_alloc, custom_free, NULL);
	//s = BrotliDecoderCreateInstance(NULL, NULL, NULL);
	defer { BrotliDecoderDestroyInstance(s); }

	size_t	total_out = 0;
	for (;;) {
		BrotliDecoderResult	result = BrotliDecoderDecompressStream(s, &available_in, &next_in, &available_out, &next_out, &total_out);

		switch (result) {
			case BROTLI_DECODER_SUCCESS:
				goto success;

			case BROTLI_DECODER_RESULT_ERROR:
				THROW_ERROR("Brotli decompress error: ", BrotliDecoderErrorString(BrotliDecoderGetErrorCode(s)));

			case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
				THROW_ERROR("Input is truncated");

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
				THROW_ERROR("Unhandled result from BrotliDecoderDecompressStream");
		}
	}

success:
	Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(out, total_out));

	return TCL_OK;
}

//>>>

extern const BrotliStubs* const brotliConstStubsPtr;

DLLEXPORT int Brotli_Init(Tcl_Interp* interp) //<<<
{
#if USE_TCL_STUBS
	if (!Tcl_InitStubs(interp, TCL_VERSION, 0)) return TCL_ERROR;
#endif

	Tcl_Namespace*	ns = Tcl_CreateNamespace(interp, NS, NULL, NULL);
	TEST_OK(Tcl_Export(interp, ns, "*", 0));

	Tcl_CreateObjCommand(interp, NS "::compress",   compress_cmd,   NULL, NULL);
	Tcl_CreateObjCommand(interp, NS "::decompress", decompress_cmd, NULL, NULL);

	TEST_OK(Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, brotliConstStubsPtr));

	return TCL_OK;
}

//>>>


// vim: foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4 noexpandtab
