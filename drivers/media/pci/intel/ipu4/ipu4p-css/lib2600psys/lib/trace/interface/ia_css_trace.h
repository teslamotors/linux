/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

/*! \file */

#ifndef __IA_CSS_TRACE_H
#define __IA_CSS_TRACE_H

/*
** Configurations
*/

/**
 * STEP 1: Define {Module Name}_TRACE_METHOD to one of the following.
 *         Where:
 *             {Module Name} is the name of the targeted module.
 *
 *         Example:
 *             #define NCI_DMA_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
 */

/**< Use whatever method of tracing that best suits the platform
 * this code is compiled for.
 */
#define IA_CSS_TRACE_METHOD_NATIVE  1
/**< Use the Tracing NCI. */
#define IA_CSS_TRACE_METHOD_TRACE   2

/**
 * STEP 2: Define {Module Name}_TRACE_LEVEL_{Level} to one of the following.
 *         Where:
 *             {Module Name} is the name of the targeted module.
 *             {Level}, in decreasing order of severity, is one of the
 *             following values:
 *             {ASSERT, ERROR, WARNING, INFO, DEBUG, VERBOSE}.
 *
 *         Example:
 *             #define NCI_DMA_TRACE_LEVEL_ASSERT IA_CSS_TRACE_LEVEL_DISABLED
 *             #define NCI_DMA_TRACE_LEVEL_ERROR  IA_CSS_TRACE_LEVEL_ENABLED
 */
/**< Disables the corresponding trace level. */
#define IA_CSS_TRACE_LEVEL_DISABLED 0
/**< Enables the corresponding trace level. */
#define IA_CSS_TRACE_LEVEL_ENABLED  1

/*
 * Used in macro definition with do-while loop
 * for removing checkpatch warnings
 */
#define IA_CSS_TRACE_FILE_DUMMY_DEFINE

/**
 * STEP 3: Define IA_CSS_TRACE_PRINT_FILE_LINE to have file name and
 * line printed with every log message.
 *
 *	   Example:
 *	       #define IA_CSS_TRACE_PRINT_FILE_LINE
 */

/*
** Interface
*/

/*
** Static
*/

/**
 * Logs a message with zero arguments if the targeted severity level is enabled
 * at compile-time.
 * @param module The targeted module.
 * @param severity The severity level of the trace message. In decreasing order:
 *                 {ASSERT, ERROR, WARNING, INFO, DEBUG, VERBOSE}.
 * @param format The message to be traced.
 */
#define IA_CSS_TRACE_0(module, severity, format) \
	IA_CSS_TRACE_IMPL(module, 0, severity, format)

/**
 * Logs a message with one argument if the targeted severity level is enabled
 * at compile-time.
 * @see IA_CSS_TRACE_0
 */
#define IA_CSS_TRACE_1(module, severity, format, a1) \
	IA_CSS_TRACE_IMPL(module, 1, severity, format, a1)

/**
 * Logs a message with two arguments if the targeted severity level is enabled
 * at compile-time.
 * @see IA_CSS_TRACE_0
 */
#define IA_CSS_TRACE_2(module, severity, format, a1, a2) \
	IA_CSS_TRACE_IMPL(module, 2, severity, format, a1, a2)

/**
 * Logs a message with three arguments if the targeted severity level
 * is enabled at compile-time.
 * @see IA_CSS_TRACE_0
 */
#define IA_CSS_TRACE_3(module, severity, format, a1, a2, a3) \
	IA_CSS_TRACE_IMPL(module, 3, severity, format, a1, a2, a3)

/**
 * Logs a message with four arguments if the targeted severity level is enabled
 * at compile-time.
 * @see IA_CSS_TRACE_0
 */
#define IA_CSS_TRACE_4(module, severity, format, a1, a2, a3, a4) \
	IA_CSS_TRACE_IMPL(module, 4, severity, format, a1, a2, a3, a4)

/**
 * Logs a message with five arguments if the targeted severity level is enabled
 * at compile-time.
 * @see IA_CSS_TRACE_0
 */
#define IA_CSS_TRACE_5(module, severity, format, a1, a2, a3, a4, a5) \
	IA_CSS_TRACE_IMPL(module, 5, severity, format, a1, a2, a3, a4, a5)

/**
 * Logs a message with six arguments if the targeted severity level is enabled
 * at compile-time.
 * @see IA_CSS_TRACE_0
 */
#define IA_CSS_TRACE_6(module, severity, format, a1, a2, a3, a4, a5, a6) \
	IA_CSS_TRACE_IMPL(module, 6, severity, format, a1, a2, a3, a4, a5, a6)

/**
 * Logs a message with seven arguments if the targeted severity level
 * is enabled at compile-time.
 * @see IA_CSS_TRACE_0
 */
#define IA_CSS_TRACE_7(module, severity, format, a1, a2, a3, a4, a5, a6, a7) \
	IA_CSS_TRACE_IMPL(module, 7, severity, format, \
					a1, a2, a3, a4, a5, a6, a7)

/*
** Dynamic
*/

/**
* Declares, but does not define, dynamic tracing functions and variables
* for module \p module.  For each module, place an instance of this macro
* in the compilation unit in which you want to use dynamic tracing facility
* so as to inform the compiler of the declaration of the available functions.
* An invocation of this function does not enable any of the available tracing
* levels.  Do not place a semicolon after a call to this macro.
* @see IA_CSS_TRACE_DYNAMIC_DEFINE
*/
#define IA_CSS_TRACE_DYNAMIC_DECLARE(module) \
	IA_CSS_TRACE_DYNAMIC_DECLARE_IMPL(module)
/**
* Declares the configuration function for the dynamic api seperatly, if one
* wants to use it.
*/
#define IA_CSS_TRACE_DYNAMIC_DECLARE_CONFIG_FUNC(module) \
	IA_CSS_TRACE_DYNAMIC_DECLARE_CONFIG_FUNC_IMPL(module)

/**
* Defines dynamic tracing functions and variables for module \p module.
* For each module, place an instance of this macro in one, and only one,
* of your SOURCE files so as to allow the linker resolve the related symbols.
* An invocation of this macro does not enable any of the available tracing
* levels.  Do not place a semicolon after a call to this macro.
* @see IA_CSS_TRACE_DYNAMIC_DECLARE
*/
#define IA_CSS_TRACE_DYNAMIC_DEFINE(module) \
	IA_CSS_TRACE_DYNAMIC_DEFINE_IMPL(module)
/**
* Defines the configuration function for the dynamic api seperatly, if one
* wants to use it.
*/
#define IA_CSS_TRACE_DYNAMIC_DEFINE_CONFIG_FUNC(module) \
	IA_CSS_TRACE_DYNAMIC_DEFINE_CONFIG_FUNC_IMPL(module)

/**
 * Logs a message with zero arguments if the targeted severity level is enabled
 * both at compile-time, and run-time.
 * @param module The targeted module.
 * @param severity The severity level of the trace message. In decreasing order:
 *                 {ASSERT, ERROR, WARNING, INFO, DEBUG, VERBOSE}.
 * @param format The message to be traced.
 */
#define IA_CSS_TRACE_DYNAMIC_0(module, severity, format) \
	IA_CSS_TRACE_DYNAMIC_IMPL(module, 0, severity, format)

/**
 * Logs a message with one argument if the targeted severity level is enabled
 * both at compile-time, and run-time.
 * @see IA_CSS_TRACE_DYNAMIC_0
 */
#define IA_CSS_TRACE_DYNAMIC_1(module, severity, format, a1) \
	IA_CSS_TRACE_DYNAMIC_IMPL(module, 1, severity, format, a1)

/**
 * Logs a message with two arguments if the targeted severity level is enabled
 * both at compile-time, and run-time.
 * @see IA_CSS_TRACE_DYNAMIC_0
 */
#define IA_CSS_TRACE_DYNAMIC_2(module, severity, format, a1, a2) \
	IA_CSS_TRACE_DYNAMIC_IMPL(module, 2, severity, format, a1, a2)

/**
 * Logs a message with three arguments if the targeted severity level
 * is enabled both at compile-time, and run-time.
 * @see IA_CSS_TRACE_DYNAMIC_0
 */
#define IA_CSS_TRACE_DYNAMIC_3(module, severity, format, a1, a2, a3) \
	IA_CSS_TRACE_DYNAMIC_IMPL(module, 3, severity, format, a1, a2, a3)

/**
 * Logs a message with four arguments if the targeted severity level is enabled
 * both at compile-time, and run-time.
 * @see IA_CSS_TRACE_DYNAMIC_0
 */
#define IA_CSS_TRACE_DYNAMIC_4(module, severity, format, a1, a2, a3, a4) \
	IA_CSS_TRACE_DYNAMIC_IMPL(module, 4, severity, format, a1, a2, a3, a4)

/**
 * Logs a message with five arguments if the targeted severity level is enabled
 * both at compile-time, and run-time.
 * @see IA_CSS_TRACE_DYNAMIC_0
 */
#define IA_CSS_TRACE_DYNAMIC_5(module, severity, format, a1, a2, a3, a4, a5) \
	IA_CSS_TRACE_DYNAMIC_IMPL(module, 5, severity, format, \
						a1, a2, a3, a4, a5)

/**
 * Logs a message with six arguments if the targeted severity level is enabled
 * both at compile-time, and run-time.
 * @see IA_CSS_TRACE_DYNAMIC_0
 */
#define IA_CSS_TRACE_DYNAMIC_6(module, severity, format, \
						a1, a2, a3, a4, a5, a6) \
	IA_CSS_TRACE_DYNAMIC_IMPL(module, 6, severity, format, \
						a1, a2, a3, a4, a5, a6)

/**
 * Logs a message with seven arguments if the targeted severity level
 * is enabled both at compile-time, and run-time.
 * @see IA_CSS_TRACE_DYNAMIC_0
 */
#define IA_CSS_TRACE_DYNAMIC_7(module, severity, format, \
						a1, a2, a3, a4, a5, a6, a7) \
	IA_CSS_TRACE_DYNAMIC_IMPL(module, 7, severity, format, \
						a1, a2, a3, a4, a5, a6, a7)

/*
** Implementation
*/

/* CAT */
#define IA_CSS_TRACE_CAT_IMPL(a, b) a ## b
#define IA_CSS_TRACE_CAT(a, b) IA_CSS_TRACE_CAT_IMPL(a, b)

/* Bridge */
#if defined(__HIVECC) || defined(__GNUC__)
#define IA_CSS_TRACE_IMPL(module, argument_count, severity, arguments ...) \
	IA_CSS_TRACE_CAT( \
		IA_CSS_TRACE_CAT( \
			IA_CSS_TRACE_CAT( \
				IA_CSS_TRACE_CAT( \
					IA_CSS_TRACE_CAT( \
						IA_CSS_TRACE_, \
						argument_count \
					), \
					_ \
				), \
				IA_CSS_TRACE_CAT( \
					module, \
					_TRACE_METHOD \
				) \
			), \
			_ \
		), \
		IA_CSS_TRACE_CAT( \
			IA_CSS_TRACE_CAT( \
				module, \
				_TRACE_LEVEL_ \
			), \
			severity \
		) \
		( \
			IA_CSS_TRACE_CAT( \
				IA_CSS_TRACE_CAT( \
					IA_CSS_TRACE_CAT( \
						IA_CSS_TRACE_SEVERITY_, \
						severity \
					), \
					_ \
				), \
				IA_CSS_TRACE_CAT( \
					module, \
					_TRACE_METHOD \
				) \
			), \
			#module, \
			## arguments \
		) \
	)

/* Bridge */
#define IA_CSS_TRACE_DYNAMIC_IMPL(module, argument_count, severity, \
							arguments ...) \
	do { \
		if (IA_CSS_TRACE_CAT(IA_CSS_TRACE_CAT(module, _trace_level_), \
							severity)) { \
			IA_CSS_TRACE_IMPL(module, argument_count, severity, \
							## arguments); \
		} \
	} while (0)
#elif defined(_MSC_VER)
#define IA_CSS_TRACE_IMPL(module, argument_count, severity, ...) \
	IA_CSS_TRACE_CAT( \
		IA_CSS_TRACE_CAT( \
			IA_CSS_TRACE_CAT( \
				IA_CSS_TRACE_CAT( \
					IA_CSS_TRACE_CAT( \
						IA_CSS_TRACE_, \
						argument_count \
					), \
					_ \
				), \
				IA_CSS_TRACE_CAT( \
					module, \
					_TRACE_METHOD \
				) \
			), \
			_ \
		), \
		IA_CSS_TRACE_CAT( \
			IA_CSS_TRACE_CAT( \
				module, \
				_TRACE_LEVEL_ \
			), \
			severity \
		) \
		( \
			IA_CSS_TRACE_CAT( \
				IA_CSS_TRACE_CAT( \
					IA_CSS_TRACE_CAT( \
						IA_CSS_TRACE_SEVERITY_, \
						severity \
					), \
					_ \
				), \
				IA_CSS_TRACE_CAT( \
					module, \
					_TRACE_METHOD \
				) \
			), \
			#module, \
			__VA_ARGS__  \
		) \
	)

/* Bridge */
#define IA_CSS_TRACE_DYNAMIC_IMPL(module, argument_count, severity, ...) \
	do { \
		if (IA_CSS_TRACE_CAT(IA_CSS_TRACE_CAT(module, _trace_level_), \
							severity)) { \
			IA_CSS_TRACE_IMPL(module, argument_count, severity, \
							__VA_ARGS__); \
		} \
	} while (0)
#endif

/*
** Native Backend
*/

#if defined(__HIVECC)
	#define IA_CSS_TRACE_PLATFORM_CELL
#elif defined(__GNUC__)
	#define IA_CSS_TRACE_PLATFORM_HOST

	#define IA_CSS_TRACE_NATIVE(severity, module, format, arguments ...) \
	do { \
		IA_CSS_TRACE_FILE_PRINT_COMMAND; \
		PRINT(IA_CSS_TRACE_FORMAT_AUG_NATIVE(severity, module, \
						format),  ## arguments); \
	} while (0)
	/* TODO: In case Host Side tracing is needed to be mapped to the
	 * Tunit, the following "IA_CSS_TRACE_TRACE" needs to be modified from
	 * PRINT to vied_nci_tunit_print function calls
	*/
	#define IA_CSS_TRACE_TRACE(severity, module, format, arguments ...) \
	do { \
		IA_CSS_TRACE_FILE_PRINT_COMMAND; \
		PRINT(IA_CSS_TRACE_FORMAT_AUG_TRACE(severity, module, \
						format),  ## arguments); \
	} while (0)

#elif defined(_MSC_VER)
	#define IA_CSS_TRACE_PLATFORM_HOST

	#define IA_CSS_TRACE_NATIVE(severity, module, format, ...) \
		do { \
			IA_CSS_TRACE_FILE_PRINT_COMMAND; \
			PRINT(IA_CSS_TRACE_FORMAT_AUG_NATIVE(severity, \
					module, format),  __VA_ARGS__); \
		} while (0)
	/* TODO: In case Host Side tracing is needed to be mapped to the
	 * Tunit, the following "IA_CSS_TRACE_TRACE" needs to be modified from
	 * PRINT to vied_nci_tunit_print function calls
	*/
	#define IA_CSS_TRACE_TRACE(severity, module, format, ...) \
		do { \
			IA_CSS_TRACE_FILE_PRINT_COMMAND; \
			PRINT(IA_CSS_TRACE_FORMAT_AUG_TRACE(severity, \
					module, format),  __VA_ARGS__); \
		} while (0)
#else
	#error Unsupported platform!
#endif /* Platform */

#if defined(IA_CSS_TRACE_PLATFORM_CELL)
	#include <hive/attributes.h> /* VOLATILE */

	#ifdef IA_CSS_TRACE_PRINT_FILE_LINE
		#define IA_CSS_TRACE_FILE_PRINT_COMMAND \
			do { \
				OP___printstring(__FILE__":") VOLATILE; \
				OP___printdec(__LINE__) VOLATILE; \
				OP___printstring("\n") VOLATILE; \
			} while (0)
	#else
		#define IA_CSS_TRACE_FILE_PRINT_COMMAND
	#endif

	#define IA_CSS_TRACE_MODULE_SEVERITY_PRINT(module, severity) \
		do { \
			IA_CSS_TRACE_FILE_DUMMY_DEFINE; \
			OP___printstring("["module"]:["severity"]:") \
			VOLATILE; \
		} while (0)

	#define IA_CSS_TRACE_MSG_NATIVE(severity, module, format) \
		do { \
			IA_CSS_TRACE_FILE_PRINT_COMMAND; \
			OP___printstring("["module"]:["severity"]: "format) \
			VOLATILE; \
		} while (0)

	#define IA_CSS_TRACE_ARG_NATIVE(module, severity, i, value) \
		do { \
			IA_CSS_TRACE_MODULE_SEVERITY_PRINT(module, severity); \
			OP___dump(i, value) VOLATILE; \
		} while (0)

	#define IA_CSS_TRACE_NATIVE_0(severity, module, format) \
		IA_CSS_TRACE_MSG_NATIVE(severity, module, format)

	#define IA_CSS_TRACE_NATIVE_1(severity, module, format, a1) \
		do { \
			IA_CSS_TRACE_MSG_NATIVE(severity, module, format); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 1, a1); \
		} while (0)

	#define IA_CSS_TRACE_NATIVE_2(severity, module, format, a1, a2) \
		do { \
			IA_CSS_TRACE_MSG_NATIVE(severity, module, format); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 1, a1); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 2, a2); \
		} while (0)

	#define IA_CSS_TRACE_NATIVE_3(severity, module, format, a1, a2, a3) \
		do { \
			IA_CSS_TRACE_MSG_NATIVE(severity, module, format); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 1, a1); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 2, a2); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 3, a3); \
		} while (0)

	#define IA_CSS_TRACE_NATIVE_4(severity, module, format, \
						a1, a2, a3, a4) \
		do { \
			IA_CSS_TRACE_MSG_NATIVE(severity, module, format); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 1, a1); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 2, a2); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 3, a3); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 4, a4); \
		} while (0)

	#define IA_CSS_TRACE_NATIVE_5(severity, module, format, \
						a1, a2, a3, a4, a5) \
		do { \
			IA_CSS_TRACE_MSG_NATIVE(severity, module, format); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 1, a1); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 2, a2); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 3, a3); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 4, a4); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 5, a5); \
		} while (0)

	#define IA_CSS_TRACE_NATIVE_6(severity, module, format, \
						a1, a2, a3, a4, a5, a6) \
		do { \
			IA_CSS_TRACE_MSG_NATIVE(severity, module, format); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 1, a1); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 2, a2); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 3, a3); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 4, a4); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 5, a5); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 6, a6); \
		} while (0)

	#define IA_CSS_TRACE_NATIVE_7(severity, module, format, \
						a1, a2, a3, a4, a5, a6, a7) \
		do { \
			IA_CSS_TRACE_MSG_NATIVE(severity, module, format); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 1, a1); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 2, a2); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 3, a3); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 4, a4); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 5, a5); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 6, a6); \
			IA_CSS_TRACE_ARG_NATIVE(module, severity, 7, a7); \
		} while (0)
	/*
	** Tracing Backend
	*/
#if !defined(HRT_CSIM) && !defined(NO_TUNIT)
	#include "vied_nci_tunit.h"
#endif
	#define IA_CSS_TRACE_AUG_FORMAT_TRACE(format, module) \
		"[" module "]" format " : PID = %x : Timestamp = %d : PC = %x"

	#define IA_CSS_TRACE_TRACE_0(severity, module, format) \
		vied_nci_tunit_print(IA_CSS_TRACE_AUG_FORMAT_TRACE(format, \
								module), \
			severity)

	#define IA_CSS_TRACE_TRACE_1(severity, module, format, a1) \
		vied_nci_tunit_print1i(IA_CSS_TRACE_AUG_FORMAT_TRACE(format, \
								module), \
			severity, a1)

	#define IA_CSS_TRACE_TRACE_2(severity, module, format, a1, a2) \
		vied_nci_tunit_print2i(IA_CSS_TRACE_AUG_FORMAT_TRACE(format, \
								module), \
			severity, a1, a2)

	#define IA_CSS_TRACE_TRACE_3(severity, module, format, a1, a2, a3) \
		vied_nci_tunit_print3i(IA_CSS_TRACE_AUG_FORMAT_TRACE(format, \
								module), \
			severity, a1, a2, a3)

	#define IA_CSS_TRACE_TRACE_4(severity, module, format, a1, a2, a3, a4) \
		vied_nci_tunit_print4i(IA_CSS_TRACE_AUG_FORMAT_TRACE(format, \
								module), \
			severity, a1, a2, a3, a4)

	#define IA_CSS_TRACE_TRACE_5(severity, module, format, \
						a1, a2, a3, a4, a5) \
		vied_nci_tunit_print5i(IA_CSS_TRACE_AUG_FORMAT_TRACE(format, \
								module), \
			severity, a1, a2, a3, a4, a5)

	#define IA_CSS_TRACE_TRACE_6(severity, module, format, \
						a1, a2, a3, a4, a5, a6) \
		vied_nci_tunit_print6i(IA_CSS_TRACE_AUG_FORMAT_TRACE(format, \
								module), \
			severity, a1, a2, a3, a4, a5, a6)

	#define IA_CSS_TRACE_TRACE_7(severity, module, format, \
						a1, a2, a3, a4, a5, a6, a7) \
		vied_nci_tunit_print7i(IA_CSS_TRACE_AUG_FORMAT_TRACE(format, \
								module), \
			severity, a1, a2, a3, a4, a5, a6, a7)

#elif defined(IA_CSS_TRACE_PLATFORM_HOST)
	#include "print_support.h"

	#ifdef IA_CSS_TRACE_PRINT_FILE_LINE
		#define IA_CSS_TRACE_FILE_PRINT_COMMAND \
				PRINT("%s:%d:\n", __FILE__, __LINE__)
	#else
		#define IA_CSS_TRACE_FILE_PRINT_COMMAND
	#endif

	#define IA_CSS_TRACE_FORMAT_AUG_NATIVE(severity, module, format) \
			"[" module "]:[" severity "]: " format

	#define IA_CSS_TRACE_NATIVE_0(severity, module, format) \
		IA_CSS_TRACE_NATIVE(severity, module, format)

	#define IA_CSS_TRACE_NATIVE_1(severity, module, format, a1) \
		IA_CSS_TRACE_NATIVE(severity, module, format, a1)

	#define IA_CSS_TRACE_NATIVE_2(severity, module, format, a1, a2) \
		IA_CSS_TRACE_NATIVE(severity, module, format, a1, a2)

	#define IA_CSS_TRACE_NATIVE_3(severity, module, format, a1, a2, a3) \
		IA_CSS_TRACE_NATIVE(severity, module, format, a1, a2, a3)

	#define IA_CSS_TRACE_NATIVE_4(severity, module, format, \
						a1, a2, a3, a4) \
		IA_CSS_TRACE_NATIVE(severity, module, format, a1, a2, a3, a4)

	#define IA_CSS_TRACE_NATIVE_5(severity, module, format, \
						a1, a2, a3, a4, a5) \
		IA_CSS_TRACE_NATIVE(severity, module, format, \
						a1, a2, a3, a4, a5)

	#define IA_CSS_TRACE_NATIVE_6(severity, module, format, \
						a1, a2, a3, a4, a5, a6) \
		IA_CSS_TRACE_NATIVE(severity, module, format, \
						a1, a2, a3, a4, a5, a6)

	#define IA_CSS_TRACE_NATIVE_7(severity, module, format, \
						a1, a2, a3, a4, a5, a6, a7) \
		IA_CSS_TRACE_NATIVE(severity, module, format, \
						a1, a2, a3, a4, a5, a6, a7)

	#define IA_CSS_TRACE_FORMAT_AUG_TRACE(severity, module, format) \
			"["module"]:["severity"]: "format

	#define IA_CSS_TRACE_TRACE_0(severity, module, format) \
		IA_CSS_TRACE_TRACE(severity, module, format)

	#define IA_CSS_TRACE_TRACE_1(severity, module, format, a1) \
		IA_CSS_TRACE_TRACE(severity, module, format, a1)

	#define IA_CSS_TRACE_TRACE_2(severity, module, format, a1, a2) \
		IA_CSS_TRACE_TRACE(severity, module, format, a1, a2)

	#define IA_CSS_TRACE_TRACE_3(severity, module, format, a1, a2, a3) \
		IA_CSS_TRACE_TRACE(severity, module, format, a1, a2, a3)

	#define IA_CSS_TRACE_TRACE_4(severity, module, format, \
						a1, a2, a3, a4) \
		IA_CSS_TRACE_TRACE(severity, module, format, a1, a2, a3, a4)

	#define IA_CSS_TRACE_TRACE_5(severity, module, format, \
						a1, a2, a3, a4, a5) \
		IA_CSS_TRACE_TRACE(severity, module, format, \
						a1, a2, a3, a4, a5)

	#define IA_CSS_TRACE_TRACE_6(severity, module, format, \
						a1, a2, a3, a4, a5, a6) \
		IA_CSS_TRACE_TRACE(severity, module, format, \
						a1, a2, a3, a4, a5, a6)

	#define IA_CSS_TRACE_TRACE_7(severity, module, format, \
						a1, a2, a3, a4, a5, a6, a7) \
		IA_CSS_TRACE_TRACE(severity, module, format, \
						a1, a2, a3, a4, a5, a6, a7)
#endif

/* Disabled */
/* Legend: IA_CSS_TRACE_{Argument Count}_{Backend ID}_{Enabled} */
#define IA_CSS_TRACE_0_1_0(severity, module, format)
#define IA_CSS_TRACE_1_1_0(severity, module, format, arg1)
#define IA_CSS_TRACE_2_1_0(severity, module, format, arg1, arg2)
#define IA_CSS_TRACE_3_1_0(severity, module, format, arg1, arg2, arg3)
#define IA_CSS_TRACE_4_1_0(severity, module, format, arg1, arg2, arg3, arg4)
#define IA_CSS_TRACE_5_1_0(severity, module, format, arg1, arg2, arg3, arg4, \
							arg5)
#define IA_CSS_TRACE_6_1_0(severity, module, format, arg1, arg2, arg3, arg4, \
							arg5, arg6)
#define IA_CSS_TRACE_7_1_0(severity, module, format, arg1, arg2, arg3, arg4, \
							arg5, arg6, arg7)

/* Enabled */
/* Legend: IA_CSS_TRACE_{Argument Count}_{Backend ID}_{Enabled} */
#define IA_CSS_TRACE_0_1_1 IA_CSS_TRACE_NATIVE_0
#define IA_CSS_TRACE_1_1_1 IA_CSS_TRACE_NATIVE_1
#define IA_CSS_TRACE_2_1_1 IA_CSS_TRACE_NATIVE_2
#define IA_CSS_TRACE_3_1_1 IA_CSS_TRACE_NATIVE_3
#define IA_CSS_TRACE_4_1_1 IA_CSS_TRACE_NATIVE_4
#define IA_CSS_TRACE_5_1_1 IA_CSS_TRACE_NATIVE_5
#define IA_CSS_TRACE_6_1_1 IA_CSS_TRACE_NATIVE_6
#define IA_CSS_TRACE_7_1_1 IA_CSS_TRACE_NATIVE_7

/* Enabled */
/* Legend: IA_CSS_TRACE_SEVERITY_{Severity Level}_{Backend ID} */
#define IA_CSS_TRACE_SEVERITY_ASSERT_1  "Assert"
#define IA_CSS_TRACE_SEVERITY_ERROR_1   "Error"
#define IA_CSS_TRACE_SEVERITY_WARNING_1 "Warning"
#define IA_CSS_TRACE_SEVERITY_INFO_1    "Info"
#define IA_CSS_TRACE_SEVERITY_DEBUG_1   "Debug"
#define IA_CSS_TRACE_SEVERITY_VERBOSE_1 "Verbose"

/* Disabled */
/* Legend: IA_CSS_TRACE_{Argument Count}_{Backend ID}_{Enabled} */
#define IA_CSS_TRACE_0_2_0(severity, module, format)
#define IA_CSS_TRACE_1_2_0(severity, module, format, arg1)
#define IA_CSS_TRACE_2_2_0(severity, module, format, arg1, arg2)
#define IA_CSS_TRACE_3_2_0(severity, module, format, arg1, arg2, arg3)
#define IA_CSS_TRACE_4_2_0(severity, module, format, arg1, arg2, arg3, arg4)
#define IA_CSS_TRACE_5_2_0(severity, module, format, arg1, arg2, arg3, arg4, \
							arg5)
#define IA_CSS_TRACE_6_2_0(severity, module, format, arg1, arg2, arg3, arg4, \
							arg5, arg6)
#define IA_CSS_TRACE_7_2_0(severity, module, format, arg1, arg2, arg3, arg4, \
							arg5, arg6, arg7)

/* Enabled */
/* Legend: IA_CSS_TRACE_{Argument Count}_{Backend ID}_{Enabled} */
#define IA_CSS_TRACE_0_2_1 IA_CSS_TRACE_TRACE_0
#define IA_CSS_TRACE_1_2_1 IA_CSS_TRACE_TRACE_1
#define IA_CSS_TRACE_2_2_1 IA_CSS_TRACE_TRACE_2
#define IA_CSS_TRACE_3_2_1 IA_CSS_TRACE_TRACE_3
#define IA_CSS_TRACE_4_2_1 IA_CSS_TRACE_TRACE_4
#define IA_CSS_TRACE_5_2_1 IA_CSS_TRACE_TRACE_5
#define IA_CSS_TRACE_6_2_1 IA_CSS_TRACE_TRACE_6
#define IA_CSS_TRACE_7_2_1 IA_CSS_TRACE_TRACE_7

/* Enabled */
/* Legend: IA_CSS_TRACE_SEVERITY_{Severity Level}_{Backend ID} */
#define IA_CSS_TRACE_SEVERITY_ASSERT_2  VIED_NCI_TUNIT_MSG_SEVERITY_FATAL
#define IA_CSS_TRACE_SEVERITY_ERROR_2   VIED_NCI_TUNIT_MSG_SEVERITY_ERROR
#define IA_CSS_TRACE_SEVERITY_WARNING_2 VIED_NCI_TUNIT_MSG_SEVERITY_WARNING
#define IA_CSS_TRACE_SEVERITY_INFO_2    VIED_NCI_TUNIT_MSG_SEVERITY_NORMAL
#define IA_CSS_TRACE_SEVERITY_DEBUG_2   VIED_NCI_TUNIT_MSG_SEVERITY_USER1
#define IA_CSS_TRACE_SEVERITY_VERBOSE_2 VIED_NCI_TUNIT_MSG_SEVERITY_USER2

/*
** Dynamicism
*/

#define IA_CSS_TRACE_DYNAMIC_DECLARE_IMPL(module) \
	do { \
		void IA_CSS_TRACE_CAT(module, _trace_assert_enable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_assert_disable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_error_enable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_error_disable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_warning_enable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_warning_disable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_info_enable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_info_disable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_debug_enable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_debug_disable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_verbose_enable)(void); \
		void IA_CSS_TRACE_CAT(module, _trace_verbose_disable)(void); \
	} while (0)

#define IA_CSS_TRACE_DYNAMIC_DECLARE_CONFIG_FUNC_IMPL(module) \
	do { \
		IA_CSS_TRACE_FILE_DUMMY_DEFINE; \
		void IA_CSS_TRACE_CAT(module, _trace_configure)\
			(int argc, const char *const *argv); \
	} while (0)

#include "platform_support.h"
#include "type_support.h"

#define IA_CSS_TRACE_DYNAMIC_DEFINE_IMPL(module) \
	static uint8_t IA_CSS_TRACE_CAT(module, _trace_level_assert); \
	static uint8_t IA_CSS_TRACE_CAT(module, _trace_level_error); \
	static uint8_t IA_CSS_TRACE_CAT(module, _trace_level_warning); \
	static uint8_t IA_CSS_TRACE_CAT(module, _trace_level_info); \
	static uint8_t IA_CSS_TRACE_CAT(module, _trace_level_debug); \
	static uint8_t IA_CSS_TRACE_CAT(module, _trace_level_verbose); \
	\
	void IA_CSS_TRACE_CAT(module, _trace_assert_enable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_assert) = 1; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_assert_disable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_assert) = 0; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_error_enable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_error) = 1; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_error_disable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_error) = 0; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_warning_enable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_warning) = 1; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_warning_disable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_warning) = 0; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_info_enable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_info) = 1; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_info_disable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_info) = 0; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_debug_enable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_debug) = 1; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_debug_disable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_debug) = 0; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_verbose_enable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_verbose) = 1; \
	} \
	\
	void IA_CSS_TRACE_CAT(module, _trace_verbose_disable)(void) \
	{ \
		IA_CSS_TRACE_CAT(module, _trace_level_verbose) = 0; \
	}

#define IA_CSS_TRACE_DYNAMIC_DEFINE_CONFIG_FUNC_IMPL(module) \
void IA_CSS_TRACE_CAT(module, _trace_configure)(const int argc, \
				const char *const *const argv) \
{ \
	int i = 1; \
	const char *levels = 0; \
	\
	while (i < argc) { \
		if (!strcmp(argv[i], "-" #module "_trace")) { \
			++i; \
			\
			if (i < argc) { \
				levels = argv[i]; \
				\
				while (*levels) { \
					switch (*levels++) { \
					case 'a': \
						IA_CSS_TRACE_CAT \
					(module, _trace_assert_enable)(); \
						break; \
						\
					case 'e': \
						IA_CSS_TRACE_CAT \
					(module, _trace_error_enable)(); \
						break; \
						\
					case 'w': \
						IA_CSS_TRACE_CAT \
					(module, _trace_warning_enable)(); \
						break; \
						\
					case 'i': \
						IA_CSS_TRACE_CAT \
					(module, _trace_info_enable)(); \
						break; \
						\
					case 'd': \
						IA_CSS_TRACE_CAT \
					(module, _trace_debug_enable)(); \
						break; \
						\
					case 'v': \
						IA_CSS_TRACE_CAT \
					(module, _trace_verbose_enable)(); \
						break; \
						\
					default: \
					} \
				} \
			} \
		} \
		\
	++i; \
	} \
}

#endif /* __IA_CSS_TRACE_H */
