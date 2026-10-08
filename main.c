#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/types.h>
#include <stdint.h>
#include <taihen.h>

#define ENABLE_LOG_MESSAGE 0
#define LOG_PATH "ux0:data/screenshot_noshard.log"

#define TARGET_MODULE "SceAvMediaService"
#define TEXT_SEGMENT 0

#define RETAIL_PATH_SOURCE_CONTEXT_OFFSET 0x00004080U
#define RETAIL_PATH_PATCH_OFFSET 0x00004084U
#define RETAIL_PATH_DESTINATION_OFFSET 0x0000421EU
#define RETAIL_PROMOTE_SCHEDULER_OFFSET 0x000054BEU
#define RETAIL_PROMOTE_COMPLETION_OFFSET 0x000053E8U

#define NONRETAIL_PATH_SOURCE_CONTEXT_OFFSET 0x000040B4U
#define NONRETAIL_PATH_PATCH_OFFSET 0x000040B8U
#define NONRETAIL_PATH_DESTINATION_OFFSET 0x00004252U
#define NONRETAIL_PROMOTE_SCHEDULER_OFFSET 0x00005550U
#define NONRETAIL_PROMOTE_COMPLETION_OFFSET 0x0000547AU

#define PHOTO_RESULT_PATH_SIZE 1024U
#define SCREENSHOT_PREFIX "photo0:/SCREENSHOT/"

typedef struct PhotoExportResult {
	char path[PHOTO_RESULT_PATH_SIZE];
	uint32_t auxiliary_low;
	uint32_t auxiliary_high;
} PhotoExportResult;

typedef void (*PhotoExportCompletion)(void *request, int operation_type,
	int execute_result, const int *result_count, const void *results);

typedef struct FirmwareProfile {
	uint32_t module_nid;
	uint32_t path_source_context_offset;
	uint32_t path_patch_offset;
	uint32_t path_destination_offset;
	uint32_t promote_scheduler_offset;
	uint32_t promote_completion_offset;
	const uint8_t *path_source_context;
	const uint8_t *path_destination_context;
} FirmwareProfile;

/*
 * Branch over the code that appends the random two-character directory to
 * both screenshot paths. The destination resumes at Sony's normal directory
 * check and physical writer.
 */
static const uint8_t g_retail_path_source_context[] = {
	0x09, 0xF0, 0x00, 0xFC,
	0x17, 0xE0,
	0x43, 0xF2, 0x08, 0x74,
	0xC8, 0xF2, 0x17, 0x14,
	0x20, 0x1C,
};

static const uint8_t g_nonretail_path_source_context[] = {
	0x09, 0xF0, 0x48, 0xFC,
	0x17, 0xE0,
	0x42, 0xF6, 0xD0, 0x34,
	0xC8, 0xF2, 0x01, 0x14,
	0x20, 0x1C,
};

static const uint8_t g_path_source_mask[] = {
	0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF,
	0xF0, 0xFB, 0x00, 0x8F,
	0xF0, 0xFB, 0x00, 0x8F,
	0xFF, 0xFF,
};

static const uint8_t g_retail_path_destination_context[] = {
	0x1C, 0x98,
	0x0A, 0xF0, 0xC2, 0xED,
	0x00, 0x28,
	0x4F, 0xF0, 0xFF, 0x35,
	0x35, 0xD1,
	0x1C, 0x98,
};

static const uint8_t g_nonretail_path_destination_context[] = {
	0x1C, 0x98,
	0x0A, 0xF0, 0x0A, 0xEE,
	0x00, 0x28,
	0x4F, 0xF0, 0xFF, 0x35,
	0x35, 0xD1,
	0x1C, 0x98,
};

static const uint8_t g_path_destination_mask[] = {
	0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,
};

/* Thumb B.n from each profile's patch offset to its destination offset. */
static const uint8_t g_path_branch[] = {0xCB, 0xE0};

/* Entry of fun_photo_export_schedule_promote_job. */
static const uint8_t g_scheduler_context[] = {
	0x2D, 0xE9, 0xF0, 0x4F, 0x9D, 0xB0,
	0x40, 0xF6, 0x98, 0x14,
	0xC8, 0xF2, 0x17, 0x14,
	0x24, 0x68, 0x1C, 0x94, 0x0A, 0x2A,
	0xCD, 0xE9, 0x19, 0x20, 0x4F, 0xF0,
	0x00, 0x00, 0x1B, 0x90, 0x10, 0xD9,
};

static const uint8_t g_scheduler_mask[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xF0, 0xFB, 0x00, 0x8F,
	0xF0, 0xFB, 0x00, 0x8F,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

/* Entry of fun_photo_export_promotion_completion_callback. */
static const uint8_t g_completion_context[] = {
	0x2D, 0xE9, 0xF0, 0x4B, 0x84, 0xB0, 0xEB, 0x46,
	0x40, 0xF6, 0x98, 0x19,
	0xC8, 0xF2, 0x17, 0x19,
	0xD9, 0xF8, 0x00, 0x40, 0xCB, 0xF8,
	0x0C, 0x40, 0x0C, 0x1C, 0x40, 0xF2,
	0x30, 0x01, 0xDB, 0xF8,
};

static const uint8_t g_completion_mask[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xF0, 0xFB, 0x00, 0x8F,
	0xF0, 0xFB, 0x00, 0x8F,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,
};

#define RETAIL_PROFILE(nid) { \
	(nid), \
	RETAIL_PATH_SOURCE_CONTEXT_OFFSET, \
	RETAIL_PATH_PATCH_OFFSET, \
	RETAIL_PATH_DESTINATION_OFFSET, \
	RETAIL_PROMOTE_SCHEDULER_OFFSET, \
	RETAIL_PROMOTE_COMPLETION_OFFSET, \
	g_retail_path_source_context, \
	g_retail_path_destination_context, \
}

#define NONRETAIL_PROFILE(nid) { \
	(nid), \
	NONRETAIL_PATH_SOURCE_CONTEXT_OFFSET, \
	NONRETAIL_PATH_PATCH_OFFSET, \
	NONRETAIL_PATH_DESTINATION_OFFSET, \
	NONRETAIL_PROMOTE_SCHEDULER_OFFSET, \
	NONRETAIL_PROMOTE_COMPLETION_OFFSET, \
	g_nonretail_path_source_context, \
	g_nonretail_path_destination_context, \
}

static const FirmwareProfile g_profiles[] = {
	/* Retail */
	RETAIL_PROFILE(0x18B3FEEFU), /* 3.60 */
	RETAIL_PROFILE(0x2708938BU), /* 3.65 */
	RETAIL_PROFILE(0xEFCA5EE7U), /* 3.67 */
	RETAIL_PROFILE(0xDE405E39U), /* 3.68 */
	RETAIL_PROFILE(0x6CF9DFFEU), /* 3.69 */
	RETAIL_PROFILE(0xDFB649A5U), /* 3.70 */
	RETAIL_PROFILE(0x3B8766B7U), /* 3.71 */
	RETAIL_PROFILE(0x42335CC5U), /* 3.72 */
	RETAIL_PROFILE(0x1E0D7085U), /* 3.73 */

	/* Testkit */
	NONRETAIL_PROFILE(0x3FE87731U), /* 3.600.011 */
	NONRETAIL_PROFILE(0x4DDE4533U), /* 3.650.011 */
	NONRETAIL_PROFILE(0x787F0022U), /* 3.670.011 */
	NONRETAIL_PROFILE(0x62BE8716U), /* 3.680.011 */
	NONRETAIL_PROFILE(0xC385918DU), /* 3.720.011 */

	/* Devkit */
	NONRETAIL_PROFILE(0x1656745FU), /* 3.600.011 */
	NONRETAIL_PROFILE(0x135F2E28U), /* 3.650.011 */
	NONRETAIL_PROFILE(0xD55DFE9CU), /* 3.670.011 */
	NONRETAIL_PROFILE(0x035EAA17U), /* 3.680.011 */
	NONRETAIL_PROFILE(0xF0897155U), /* 3.710.011 */
	NONRETAIL_PROFILE(0x7AC573E8U), /* 3.730.011 */
};

#undef NONRETAIL_PROFILE
#undef RETAIL_PROFILE

static tai_hook_ref_t g_scheduler_hook_ref;
static SceUID g_scheduler_hook_uid = -1;
static SceUID g_path_injection_uid = -1;
static PhotoExportCompletion g_completion;

_Static_assert(sizeof(PhotoExportResult) == 1032U,
	"photo export result ABI changed");
_Static_assert(RETAIL_PATH_PATCH_OFFSET ==
	RETAIL_PATH_SOURCE_CONTEXT_OFFSET + 4U,
	"retail source context must contain the branch at byte four");
_Static_assert(NONRETAIL_PATH_PATCH_OFFSET ==
	NONRETAIL_PATH_SOURCE_CONTEXT_OFFSET + 4U,
	"non-retail source context must contain the branch at byte four");
_Static_assert(RETAIL_PATH_DESTINATION_OFFSET -
	(RETAIL_PATH_PATCH_OFFSET + 4U) == 0x196U,
	"retail path branch displacement changed");
_Static_assert(NONRETAIL_PATH_DESTINATION_OFFSET -
	(NONRETAIL_PATH_PATCH_OFFSET + 4U) == 0x196U,
	"non-retail path branch displacement changed");
_Static_assert(sizeof(g_retail_path_source_context) ==
	sizeof(g_path_source_mask),
	"retail path source context and mask sizes must match");
_Static_assert(sizeof(g_nonretail_path_source_context) ==
	sizeof(g_path_source_mask),
	"non-retail path source context and mask sizes must match");
_Static_assert(sizeof(g_retail_path_destination_context) ==
	sizeof(g_path_destination_mask),
	"retail path destination context and mask sizes must match");
_Static_assert(sizeof(g_nonretail_path_destination_context) ==
	sizeof(g_path_destination_mask),
	"non-retail path destination context and mask sizes must match");
_Static_assert(sizeof(g_scheduler_context) == sizeof(g_scheduler_mask),
	"scheduler context and mask sizes must match");
_Static_assert(sizeof(g_completion_context) == sizeof(g_completion_mask),
	"completion context and mask sizes must match");

static void log_message(const char *message)
{
#if ENABLE_LOG_MESSAGE
	SceUID fd;
	SceSize length;

	fd = sceIoOpen(LOG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND,
		0666);
	if (fd < 0)
		return;

	length = sceClibStrnlen(message, 512);
	if (length > 0)
		(void)sceIoWrite(fd, message, length);
	(void)sceIoClose(fd);
#else
	(void)message;
#endif
}

static int context_matches(const volatile uint8_t *actual,
	const uint8_t *expected, const uint8_t *mask, SceSize size)
{
	SceSize index;

	for (index = 0; index < size; ++index) {
		if ((actual[index] & mask[index]) !=
			(expected[index] & mask[index]))
			return 0;
	}

	return 1;
}

static int verify_context(const SceKernelSegmentInfo *text_segment,
	uint32_t offset, const uint8_t *expected, const uint8_t *mask,
	SceSize size)
{
	uintptr_t text_start = (uintptr_t)text_segment->vaddr;
	SceSize text_size = text_segment->memsz;
	const volatile uint8_t *actual;

	if (offset > text_size || size > text_size - offset)
		return -1;

	actual = (const volatile uint8_t *)(text_start + offset);
	return context_matches(actual, expected, mask, size) ? 0 : -1;
}

static const FirmwareProfile *find_profile(uint32_t module_nid)
{
	SceSize index;

	for (index = 0; index < sizeof(g_profiles) / sizeof(g_profiles[0]);
		++index) {
		if (g_profiles[index].module_nid == module_nid)
			return &g_profiles[index];
	}

	return NULL;
}

static int has_screenshot_prefix(const char *path)
{
	if (path == NULL)
		return 0;

	return sceClibStrncmp(path, SCREENSHOT_PREFIX,
		sizeof(SCREENSHOT_PREFIX) - 1U) == 0;
}

static int promote_scheduler_hook(void *service_state, void *request,
	unsigned int item_count, const void *completed_exports)
{
	PhotoExportResult result;
	const char *path = completed_exports;
	SceSize path_length;
	int result_count = 1;

	if (item_count != 1U || request == NULL || g_completion == NULL ||
		!has_screenshot_prefix(path)) {
		return TAI_CONTINUE(int, g_scheduler_hook_ref, service_state,
			request, item_count, completed_exports);
	}

	path_length = sceClibStrnlen(path, sizeof(result.path));
	if (path_length >= sizeof(result.path)) {
		return TAI_CONTINUE(int, g_scheduler_hook_ref, service_state,
			request, item_count, completed_exports);
	}

	sceClibMemset(&result, 0, sizeof(result));
	sceClibMemcpy(result.path, path, path_length);
	result.path[path_length] = '\0';

	/*
	 * Screenshots use operation type 0. Sony's completion adapter already
	 * supports synchronous invocation on pre-job failures. Passing a success
	 * result here sends the normal IPMI response and releases request-owned
	 * state without allocating or queueing ScePhotoPromote.
	 */
	log_message("[screenshot_noshard] skipping ScePhotoPromote job\n");
	g_completion(request, 0, 0, &result_count, &result);
	return 0;
}

static void release_changes(void)
{
	if (g_scheduler_hook_uid >= 0) {
		(void)taiHookRelease(g_scheduler_hook_uid, g_scheduler_hook_ref);
		g_scheduler_hook_uid = -1;
	}
	if (g_path_injection_uid >= 0) {
		(void)taiInjectRelease(g_path_injection_uid);
		g_path_injection_uid = -1;
	}
	g_completion = NULL;
}

static int install_changes(void)
{
	tai_module_info_t tai_info = {0};
	SceKernelModuleInfo module_info = {0};
	const FirmwareProfile *profile;
	const SceKernelSegmentInfo *text_segment;
	const volatile uint8_t *patched_bytes;
	uintptr_t text_start;
	int result;

	tai_info.size = sizeof(tai_info);
	result = taiGetModuleInfo(TARGET_MODULE, &tai_info);
	if (result < 0)
		return result;
	profile = find_profile(tai_info.module_nid);
	if (profile == NULL)
		return -1;

	module_info.size = sizeof(module_info);
	result = sceKernelGetModuleInfo(tai_info.modid, &module_info);
	if (result < 0)
		return result;

	text_segment = &module_info.segments[TEXT_SEGMENT];
	if (text_segment->vaddr == NULL)
		return -1;

	result = verify_context(text_segment,
		profile->path_source_context_offset,
		profile->path_source_context, g_path_source_mask,
		sizeof(g_retail_path_source_context));
	if (result < 0)
		return result;
	result = verify_context(text_segment,
		profile->path_destination_offset,
		profile->path_destination_context, g_path_destination_mask,
		sizeof(g_retail_path_destination_context));
	if (result < 0)
		return result;
	result = verify_context(text_segment,
		profile->promote_scheduler_offset,
		g_scheduler_context, g_scheduler_mask,
		sizeof(g_scheduler_context));
	if (result < 0)
		return result;
	result = verify_context(text_segment,
		profile->promote_completion_offset,
		g_completion_context, g_completion_mask,
		sizeof(g_completion_context));
	if (result < 0)
		return result;

	text_start = (uintptr_t)text_segment->vaddr;
	g_completion = (PhotoExportCompletion)
		((text_start + profile->promote_completion_offset) | 1U);

	g_path_injection_uid = taiInjectData(tai_info.modid, TEXT_SEGMENT,
		profile->path_patch_offset, g_path_branch, sizeof(g_path_branch));
	if (g_path_injection_uid < 0) {
		result = g_path_injection_uid;
		g_path_injection_uid = -1;
		g_completion = NULL;
		return result;
	}

	patched_bytes = (const volatile uint8_t *)
		(text_start + profile->path_patch_offset);
	if (patched_bytes[0] != g_path_branch[0] ||
		patched_bytes[1] != g_path_branch[1]) {
		release_changes();
		return -1;
	}

	g_scheduler_hook_uid = taiHookFunctionOffset(&g_scheduler_hook_ref,
		tai_info.modid, TEXT_SEGMENT,
		profile->promote_scheduler_offset, 1,
		promote_scheduler_hook);
	if (g_scheduler_hook_uid < 0) {
		result = g_scheduler_hook_uid;
		g_scheduler_hook_uid = -1;
		release_changes();
		return result;
	}

	log_message("[screenshot_noshard] strict AvMedia hooks installed\n");
	return 0;
}

int module_start(SceSize argc, const void *args)
{
	(void)argc;
	(void)args;

	return install_changes() < 0 ? SCE_KERNEL_START_FAILED :
		SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc;
	(void)args;

	release_changes();
	return SCE_KERNEL_STOP_SUCCESS;
}
