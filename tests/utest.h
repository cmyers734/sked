#ifndef UTEST_H
#define UTEST_H

#define Test(name, suite) \
	void test_run_##name(TestCase *test); \
	TestCase test_##name((const char *)#name, (utest_run_fcn_t)test_run_##name); \
	TestCaseAdder adder_##name(&suite, &test_##name); \
	void test_run_##name(TestCase *test)

#define assertTrue(condition) if(test->_assertTrue(NULL, (condition), __FILE__, __LINE__)) return;

#define assertEquals(expected, actual) if(test->_assertEquals(NULL, expected, actual, __FILE__, __LINE__)) return;

#define assertEqualsMsg(msg, expected, actual) if(test->_assertEquals(msg, expected, actual, __FILE__, __LINE__)) return;

#define fail(why) if (test->_fail(why, __FILE__, __LINE__)) return;

#define finish() \
    Serial.print('\x03'); \
    Serial.flush(); \
    exit(0);

#ifndef UTEST_MAX_TESTS
#define UTEST_MAX_TESTS 64
#endif

class TestSuite;
class TestCase;

typedef enum {
	UTEST_PASS = 0,
	UTEST_FAIL,
	UTEST_ERROR
} utest_test_result_e;

typedef void (*utest_run_fcn_t)(TestCase *test);

class TestSuite {
private:
	TestCase *_tests[UTEST_MAX_TESTS];
	uint8_t _num_tests;
	uint8_t _tests_run;
	uint8_t _tests_passed;
	uint8_t _tests_failed;
	uint8_t _tests_errored;
	bool _stop_on_fail;
	Stream *_print_stream;

public:
	TestSuite(void);
	void setup(void);
	void addTest(TestCase *test);
	void run(void);
	void reportFailure(const char *why, const char *file, int line);
	void printResults(void);
};

class TestCase {
private:
    const char *_name;

	TestSuite *_suite;
	utest_test_result_e _result;
	utest_run_fcn_t _run_fcn;
	
public:
	TestCase(const char *name, utest_run_fcn_t run_fcn);
	void setup(void);
	utest_test_result_e run(TestSuite *suite);
	int8_t _assertEquals(char *msg, uint32_t expected, uint32_t actual, 
			const char *file, int line);
	int8_t _assertTrue(char *msg, bool expected, const char *file, int line);
	int8_t _fail(const char *why, const char *file, int line);
};

class TestCaseAdder {
public:
	TestCaseAdder(TestSuite *suite, TestCase *test) {
		suite->addTest(test);
	}
};

TestCase::TestCase(const char *name, utest_run_fcn_t run_fcn) {
	_name = name;
	_run_fcn = run_fcn;
}
	
utest_test_result_e TestCase::run(TestSuite *suite) {
	_result = UTEST_PASS;
	_suite = suite;
	
	_run_fcn(this);

	return _result;
}

int8_t TestCase::_assertTrue(char *msg, bool expected, const char *file, int line) {
	if (!expected) {
		char bufExpected[256] = {0};
		if (msg == NULL) {
			snprintf(bufExpected, sizeof(bufExpected), "Assertion failed");
		} else {
			snprintf(bufExpected, sizeof(bufExpected), 
				"Assertion failed.\n\t%s", msg);
		}
		_fail(bufExpected, file, line);

		return 1;
	}

	return 0;
}

int8_t TestCase::_assertEquals(char *msg, uint32_t expected, uint32_t actual, 
		const char *file, int line) {
	if (expected != actual) {
		char bufExpected[256] = {0};
		if (msg == NULL) {
			snprintf(bufExpected, sizeof(bufExpected), "Assertion failed. Expected %lu, but got %lu", 
				expected, actual);
		} else {
			snprintf(bufExpected, sizeof(bufExpected), 
				"Assertion failed. Expected %lu, but got %lu\n\t%s", 
				expected, actual, msg);
		}
		_fail(bufExpected, file, line);

		return 1;
	}

	return 0;
}

int8_t TestCase::_fail(const char *why, const char *file, int line) {
	_result = UTEST_FAIL;
	_suite->reportFailure(why, file, line);
	return 1;
}

TestSuite::TestSuite(void) {
	_num_tests = 0;

	_stop_on_fail = false;

	_print_stream = &Serial;

	_tests_run = 0;
	_tests_passed = 0;
	_tests_failed = 0;
	_tests_errored = 0;
}

void TestSuite::setup(void) {
}

void TestSuite::addTest(TestCase *test) {
	if (_num_tests >= UTEST_MAX_TESTS) {
		// TODO
		return;
	}

	_tests[_num_tests++] = test;
}

void TestSuite::run(void) {
	/* For each test, run. If fail, stop. */
	for (uint16_t i = 0; i < _num_tests; i++) {
		utest_test_result_e result = _tests[i]->run(this);
		_tests_run++;

		switch (result) {
			case UTEST_PASS:
				_tests_passed++;
				break;

			case UTEST_FAIL:
				_tests_failed++;
				break;

			case UTEST_ERROR:
				_tests_errored++;
				break;
		}

		if (result != UTEST_PASS && _stop_on_fail) {
			/* Don't continue after a non-pass */
			break;
		}
	}

	printResults();
}

void TestSuite::reportFailure(const char *why, const char *file, int line) {
	_print_stream->print("!!! Test failure in ");
	_print_stream->print(file);
	_print_stream->print(":");
	_print_stream->println(line);
	_print_stream->print("!!!   ");
	_print_stream->println(why);
}

void TestSuite::printResults(void) {
	_print_stream->println("### Test Summary:");
	_print_stream->print("###  Passed:  ");
	_print_stream->println(_tests_passed);
	_print_stream->print("###  Failed:  ");
	_print_stream->println(_tests_failed);
	_print_stream->print("###  Errored: ");
	_print_stream->println(_tests_errored);
}

#endif /* #ifndef UTEST_H */
