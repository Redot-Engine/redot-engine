#include "../linear.h"

#include "./test-runner.h"

#include <type_traits>

template<class ArrayLike>
auto getAssignable(ArrayLike &arr, size_t i) -> decltype(arr[i]) & {
	return arr[i];
}

template<typename V>
struct ComplexProxy {
	V &real, &imag;
	
	ComplexProxy(V &real, V &imag) : real(real), imag(imag) {}
	
	operator std::complex<V>() const {
		return {real, imag};
	}
	
	ComplexProxy & operator=(std::complex<V> v) {
		real = v.real();
		imag = v.imag();
		return *this;
	}
};

template<typename V>
ComplexProxy<V> getAssignable(signalsmith::linear::SplitPointer<V> pointer, size_t i) {
	return {pointer.real[i], pointer.imag[i]};
}

template<class Op, int typeA, int typeB>
struct OpVoidAB {
	Op op;

	template<class Data>
	void reference(Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		for (size_t i = 0; i < data.size; ++i) {
			auto &&ai = getAssignable(a, i);
			op.opRef(ai, b[i]);
		}
	}

	template<class L, class Data>
	void linear(L &linear, Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		op.op(linear, linear.wrap(a, data.size), linear.wrap(b, data.size));
	}
};

template<class Op, int typeA, int typeB>
struct OpVoidABk {
	Op op;

	template<class Data>
	void reference(Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1)[0]; // scalar value
		for (size_t i = 0; i < data.size; ++i) {
			auto &&ai = getAssignable(a, i);
			op.opRef(ai, b);
		}
	}

	template<class L, class Data>
	void linear(L &linear, Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1)[0]; // scalar value
		op.op(linear, linear.wrap(a, data.size), b);
	}
};

template<class Op, int typeA, int typeB, int typeC>
struct OpVoidABC {
	Op op;

	template<class Data>
	void reference(Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		auto c = data.template get<typeC>(2);
		for (size_t i = 0; i < data.size; ++i) {
			auto &&ai = getAssignable(a, i);
			op.opRef(ai, b[i], c[i]);
		}
	}

	template<class L, class Data>
	void linear(L &linear, Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		auto c = data.template get<typeC>(2);
		size_t size = data.size;
		op.op(linear, linear(a, size), linear(b, size), linear(c, size));
	}
};

template<class Op, int typeA, int typeB, int typeC>
struct OpVoidABCk {
	Op op;

	template<class Data>
	void reference(Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		auto c = data.template get<typeC>(2)[0];
		for (size_t i = 0; i < data.size; ++i) {
			auto &&ai = getAssignable(a, i);
			op.opRef(ai, b[i], c);
		}
	}

	template<class L, class Data>
	void linear(L &linear, Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		auto c = data.template get<typeC>(2)[0];
		size_t size = data.size;
		op.op(linear, linear(a, size), linear(b, size), c);
	}
};

template<class Op, int typeA, int typeB, int typeC>
struct OpVoidABkC {
	Op op;

	template<class Data>
	void reference(Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1)[0];
		auto c = data.template get<typeC>(2);
		for (size_t i = 0; i < data.size; ++i) {
			auto &&ai = getAssignable(a, i);
			op.opRef(ai, b, c[i]);
		}
	}

	template<class L, class Data>
	void linear(L &linear, Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1)[0];
		auto c = data.template get<typeC>(2);
		size_t size = data.size;
		op.op(linear, linear(a, size), b, linear(c, size));
	}
};

template<class Op, int typeA, int typeB, int typeC, int typeD>
struct OpVoidABCD {
	Op op;

	template<class Data>
	void reference(Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		auto c = data.template get<typeC>(2);
		auto d = data.template get<typeD>(3);
		for (size_t i = 0; i < data.size; ++i) {
			auto &&ai = getAssignable(a, i);
			op.opRef(ai, b[i], c[i], d[i]);
		}
	}

	template<class L, class Data>
	void linear(L &linear, Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		auto c = data.template get<typeC>(2);
		auto d = data.template get<typeD>(3);
		size_t size = data.size;
		op.op(linear, linear(a, size), linear(b, size), linear(c, size), linear(d, size));
	}
};

template<class Op, int typeA, int typeB, int typeC, int typeD, int typeE>
struct OpVoidABCDE {
	Op op;

	template<class Data>
	void reference(Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		auto c = data.template get<typeC>(2);
		auto d = data.template get<typeD>(3);
		auto e = data.template get<typeE>(4);
		for (size_t i = 0; i < data.size; ++i) {
			auto &&ai = getAssignable(a, i);
			op.opRef(ai, b[i], c[i], d[i], e[i]);
		}
	}

	template<class L, class Data>
	void linear(L &linear, Data &data) const {
		auto a = data.template get<typeA>(0);
		auto b = data.template get<typeB>(1);
		auto c = data.template get<typeC>(2);
		auto d = data.template get<typeD>(3);
		auto e = data.template get<typeE>(4);
		size_t size = data.size;
		op.op(linear, linear(a, size), linear(b, size), linear(c, size), linear(d, size), linear(e, size));
	}
};

struct TestLinear {
	static const int bigPlotWidth = 300, bigPlotHeight = 250;
	static const int tinyPlotWidth = 80, tinyPlotHeight = 80;
	int tinyPlotIndex = 0;
	static const int tinyPlotColumns = 8;
	signalsmith::plot::Plot2D *firstTinyPlot = nullptr;
	int maxSize;
	double benchmarkSeconds;

	static double nToX(int n) {
		return (n >= 1) ? std::log(n) + 1 : n;
	}

	void addTicks(signalsmith::plot::Plot2D &plot) {
		plot.y.blank().major(0, "");
		plot.x.major(nToX(1), "1");
		for (int n = 4; n <= maxSize; n *= 4) {
			int log = std::round(std::log2(n));
			std::string direct = std::to_string(n);
			std::string sci = "2^" + std::to_string(log);
			std::string &label = (direct.size() <= sci.size()) ? direct : sci;
			plot.x.tick(nToX(n/2), "");
			plot.x.tick(nToX(n), label);
		}
	}

	std::string prefix;
	signalsmith::plot::Figure figure;
	signalsmith::plot::Figure tinyFigure;
	struct Config {
		std::string name;
		signalsmith::plot::Plot2D &plot;
	};
	std::vector<Config> configs;
	void addConfig(int x, int y, const char *name) {
		auto &newPlot = figure(x, y).plot(bigPlotWidth, bigPlotHeight);
		configs.push_back({name, newPlot});
		auto &firstPlot = configs[0].plot;
		if (configs.size() == 1) {
			addTicks(firstPlot);
		} else {
			newPlot.x.linkFrom(firstPlot.x);
			newPlot.y.linkFrom(firstPlot.y);
		}
		newPlot.title(name);
	}
	signalsmith::plot::Legend *legend;
	
	TestLinear(int maxSize, double benchmarkSeconds, std::string name="linear") : maxSize(maxSize), benchmarkSeconds(benchmarkSeconds), prefix(name) {
		std::cout << name << ":\n";
		addConfig(2, 0, "float (Linear)");
		addConfig(1, 0, "float (fallback)");
		addConfig(0, 0, "float (ref)");
		addConfig(2, 1, "double (Linear)");
		addConfig(1, 1, "double (fallback)");
		addConfig(0, 1, "double (ref)");
		legend = &configs[0].plot.legend(2, 1);
		tinyFigure.style.titlePadding = 0;
	}
	TestLinear(const TestLinear &other) = delete;
	~TestLinear() {
		if (benchmarkSeconds) {
			figure.write(prefix + "-all.svg");
			tinyFigure.write(prefix + "-comparison.svg");
		}
	}

	template<class OpWithArgs>
	void addOp(std::string plotName, std::string nameSuffix="") {
		OpWithArgs opWithArgs;
		std::string opName = opWithArgs.op.name;
		if (nameSuffix.size() > 0) {
			opName += " [" + nameSuffix + "]";
		}
		for (size_t i = plotName.size(); i < 12; ++i) std::cout << " ";
		std::cout << plotName;
		std::cout << " : " << opName << "\n";

		auto &tinyPlot = tinyFigure(tinyPlotIndex%tinyPlotColumns, tinyPlotIndex/tinyPlotColumns).plot(tinyPlotWidth, tinyPlotHeight);
		tinyPlot.title(plotName, 0.5, -1);
		if (!firstTinyPlot) {
			tinyPlot.x.blank();
			tinyPlot.y.blank().major(0, "");
			firstTinyPlot = &tinyPlot;
		} else {
			tinyPlot.x.linkFrom(firstTinyPlot->x);
			tinyPlot.y.linkFrom(firstTinyPlot->y);
		}
		++tinyPlotIndex;
		signalsmith::plot::Plot2D opPlot(bigPlotWidth*1.5, bigPlotHeight);
		auto &opLegend = opPlot.legend(2, 1);
		addTicks(opPlot);
		opPlot.title(opName);
		struct OpLine {
			signalsmith::plot::Line2D &main;
			signalsmith::plot::Line2D &op;
			signalsmith::plot::Line2D &tiny;
			
			void add(int n, double y) {
				double x = nToX(n);
				main.add(x, y);
				op.add(x, y);
				tiny.add(x, y);
			}
		};
		std::vector<OpLine> opLines;
		while (opLines.size() < configs.size()) {
			auto &config = configs[opLines.size()];
			auto &opPlotLine = opPlot.line();
			opLegend.add(opPlotLine, config.name);
			auto &mainLine = config.plot.line();
			auto &tinyLine = tinyPlot.line();
			opLines.push_back(OpLine{mainLine, opPlotLine, tinyLine});
		}
		legend->add(opLines[0].main, opName);
		
		signalsmith::linear::Linear linear;
		signalsmith::linear::LinearImpl<false> linearFallback;
		auto runSize = [&](int n){
			std::cout << "\tn = " << n << "\r" << std::flush;
			linear.reserve<float>(n);
			linear.reserve<double>(n);
			linearFallback.reserve<float>(n);
			linearFallback.reserve<double>(n);

			RunData<double> dataDouble(n);
			RunData<float> dataFloat(n);

			RunData<double> refDataDouble = dataDouble;
			RunData<float> refDataFloat = dataFloat;
			opWithArgs.reference(refDataDouble);
			opWithArgs.reference(refDataFloat);

			{ // check the Fallback matches
				RunData<double> copyDouble(n);
				RunData<float> copyFloat(n);
				opWithArgs.linear(linearFallback, copyDouble);
				opWithArgs.linear(linearFallback, copyFloat);
				if (copyDouble.distance(refDataDouble) > 1e-12*n) {
					std::cout << "fallback double: n = " << n << ", error = " << copyDouble.distance(refDataDouble) << "\n";
					std::cout << "\nReference:\n";
					refDataDouble.log();
					std::cout << "\nFallback:\n";
					copyDouble.log();
					abort();
				}
				if (copyFloat.distance(refDataFloat) > 1e-6*n) {
					std::cout << "fallback float: n = " << n << ", error = " << copyFloat.distance(refDataFloat) << "\n";
					std::cout << "\nReference:\n";
					refDataFloat.log();
					std::cout << "\nFallback:\n";
					copyFloat.log();
					abort();
				}
			}
			{ // check the Linear matches
				RunData<double> copyDouble(n);
				RunData<float> copyFloat(n);
				opWithArgs.linear(linear, copyDouble);
				opWithArgs.linear(linear, copyFloat);
				if (copyDouble.distance(refDataDouble) > 1e-12*n) {
					std::cout << "Linear double: n = " << n << ", error = " << copyDouble.distance(refDataDouble) << "\n";
					std::cout << "\nReference:\n";
					refDataDouble.log();
					std::cout << "\nLinear:\n";
					copyDouble.log();
					abort();
				}
				if (copyFloat.distance(refDataFloat) > 1e-6*n) {
					std::cout << "Linear float: n = " << n << ", error = " << copyFloat.distance(refDataFloat) << "\n";
					std::cout << "\nReference:\n";
					refDataFloat.log();
					std::cout << "\nLinear:\n";
					copyFloat.log();
					abort();
				}
			}
			
			double refTime = n*1e-8;
			{
				RunData<float> copy = dataFloat;
				opLines[0].add(n, refTime*runBenchmark(benchmarkSeconds, [&](){
					opWithArgs.linear(linear, copy);
				}));
			}
			{
				RunData<float> copy = dataFloat;
				opLines[1].add(n, refTime*runBenchmark(benchmarkSeconds, [&](){
					opWithArgs.linear(linearFallback, copy);
				}));
			}
			{
				RunData<float> copy = dataFloat;
				opLines[2].add(n, refTime*runBenchmark(benchmarkSeconds, [&](){
					opWithArgs.reference(copy);
				}));
			}
			{
				RunData<double> copy = dataDouble;
				opLines[3].add(n, refTime*runBenchmark(benchmarkSeconds, [&](){
					opWithArgs.linear(linear, copy);
				}));
			}
			{
				RunData<double> copy = dataDouble;
				opLines[4].add(n, refTime*runBenchmark(benchmarkSeconds, [&](){
					opWithArgs.linear(linearFallback, copy);
				}));
			}
			{
				RunData<double> copy = dataDouble;
				opLines[5].add(n, refTime*runBenchmark(benchmarkSeconds, [&](){
					opWithArgs.reference(copy);
				}));
			}
		};
		for (int n = 1; n <= maxSize; n *= 2) {
			if (n >= 8) runSize(int(n*2/M_PI));
			if (n >= 4) runSize(int(n*M_PI/4));
			runSize(n);
		}
		std::cout << "\t                                \r" << std::flush;
		
		if (benchmarkSeconds) {
			opPlot.write(prefix + "-expr-" + plotName + ".svg");
		}
	}
};

float toVal(float v) {return v;}
double toVal(double v) {return v;}
std::complex<float> toVal(std::complex<float> v) {return v;}
std::complex<double> toVal(std::complex<double> v) {return v;}

#define TEST_EXPR2(Name, refExpr, expr) \
struct Name { \
	const char *name = #expr; \
	template<class A, class B> \
	void opRef(A &a, B &&rb) const { \
		auto b = toVal(rb); \
		refExpr; \
	} \
	template<class L, class A, class B> \
	void op(L &&linear, A &&a, B &&b) const { \
		(void)linear; \
		expr; \
	} \
};
#define TEST_EXPR3(Name, refExpr, expr) \
struct Name { \
	const char *name = #expr; \
	template<class A, class B, class C> \
	void opRef(A &a, B &&rb, C &&rc) const { \
		auto b = toVal(rb); \
		auto c = toVal(rc); \
		refExpr; \
	} \
	template<class L, class A, class B, class C> \
	void op(L &&linear, A &&a, B &&b, C &&c) const { \
		(void)linear; \
		expr; \
	} \
};
#define TEST_EXPR4(Name, refExpr, expr) \
struct Name { \
	const char *name = #expr; \
	template<class A, class B, class C, class D> \
	void opRef(A &a, B &&rb, C &&rc, D &&rd) const { \
		auto b = toVal(rb); \
		auto c = toVal(rc); \
		auto d = toVal(rd); \
		refExpr; \
	} \
	template<class L, class A, class B, class C, class D> \
	void op(L &&linear, A &&a, B &&b, C &&c, D &&d) const { \
		(void)linear; \
		expr; \
	} \
};
#define TEST_EXPR5(Name, refExpr, expr) \
struct Name { \
	const char *name = #expr; \
	template<class A, class B, class C, class D, class E> \
	void opRef(A &a, B &&rb, C &&rc, D &&rd, E &&re) const { \
		auto b = toVal(rb); \
		auto c = toVal(rc); \
		auto d = toVal(rd); \
		auto e = toVal(re); \
		refExpr; \
	} \
	template<class L, class A, class B, class C, class D, class E> \
	void op(L &&linear, A &&a, B &&b, C &&c, D &&d, E &&e) const { \
		(void)linear; \
		expr; \
	} \
};
TEST_EXPR2(Assign, a = b, a = b);
TEST_EXPR3(Add, a = b + c, a = b + c);
TEST_EXPR3(Sub, a = b - c, a = b - c);
TEST_EXPR3(Mul, a = b*c, a = b*c);
TEST_EXPR3(Div, a = b/c, a = b/c);

TEST_EXPR2(Abs, a = std::abs(b), a = b.abs());
TEST_EXPR2(Neg, a = -b, a = -b);
TEST_EXPR2(Norm, a = std::norm(b), a = b.norm());
TEST_EXPR2(Exp, a = std::exp(b), a = b.exp());
TEST_EXPR2(Exp2, a = std::exp2(b), a = b.exp2());
TEST_EXPR2(Log, a = std::log(b), a = b.log());
TEST_EXPR2(Log2, a = std::log2(b), a = b.log2());
TEST_EXPR2(Log10, a = std::log10(b), a = b.log10());
TEST_EXPR2(Sqrt, a = std::sqrt(b), a = b.sqrt());
TEST_EXPR2(Cbrt, a = std::cbrt(b), a = b.cbrt());
TEST_EXPR2(SqrtNorm, a = std::sqrt(std::norm(b)), a = b.norm().sqrt());
TEST_EXPR2(Conj, a = std::conj(b), a = b.conj());
TEST_EXPR2(Real, a = std::real(b), a = b.real());
TEST_EXPR2(Imag, a = std::imag(b), a = b.imag());
TEST_EXPR2(Arg, a = std::arg(b), a = b.arg());
TEST_EXPR2(Floor, a = std::floor(b), a = b.floor());
TEST_EXPR2(Ceil, a = std::ceil(b), a = b.ceil());
TEST_EXPR2(MinusFloor, a = b - std::floor(b), a = b - b.floor());

TEST_EXPR2(AbsNeg, a = -std::abs(b), a = -b.abs());
TEST_EXPR4(MulAdd, a = b*c + d, a = b*c + d);
TEST_EXPR4(MulAdd2, a = b + c*d, a = b + c*d);
TEST_EXPR4(AddMul, a = b*(c + d), a = b*(c + d));
TEST_EXPR4(AddMul2, a = (b + c)*d, a = (b + c)*d);
TEST_EXPR4(MulSub, a = b*c - d, a = b*c - d);
TEST_EXPR4(MulSub2, a = b - c*d, a = b - c*d);
TEST_EXPR4(SubMul, a = b*(c - d), a = b*(c - d));
TEST_EXPR4(SubMul2, a = (b - c)*d, a = (b - c)*d);
TEST_EXPR5(AddAddMul, a = (b + c)*(d + e), a = (b + c)*(d + e));
TEST_EXPR5(AddSubMul, a = (b + c)*(d - e), a = (b + c)*(d - e));
TEST_EXPR5(SubAddMul, a = (b - c)*(d + e), a = (b - c)*(d + e));
TEST_EXPR5(SubSubMul, a = (b - c)*(d - e), a = (b - c)*(d - e));
TEST_EXPR5(MulMulAdd, a = b*c + d*e, a = b*c + d*e);
TEST_EXPR5(MulMulSub, a = b*c - d*e, a = b*c - d*e);

TEST_EXPR3(Max, a = std::max(b, c), a = linear.max(b, c));
TEST_EXPR3(Min, a = std::min(b, c), a = linear.min(b, c));

void testLinear(int maxSize, double benchmarkSeconds) {
	std::cout << "\nExpressions\n-----------\n";
	{
		TestLinear test(maxSize, benchmarkSeconds, "real");
		
		test.addOp<OpVoidAB<Assign, 0, 0>>("Assign");
		test.addOp<OpVoidAB<Abs, 0, 0>>("Abs");
		test.addOp<OpVoidAB<Neg, 0, 0>>("Neg");
		
		test.addOp<OpVoidABC<Add, 0, 0, 0>>("Add");
		test.addOp<OpVoidABC<Sub, 0, 0, 0>>("Sub");
		test.addOp<OpVoidABC<Mul, 0, 0, 0>>("Mul");
		test.addOp<OpVoidABC<Div, 0, 0, 0>>("Div");

		test.addOp<OpVoidAB<Exp, 0, 0>>("Exp");
		test.addOp<OpVoidAB<Exp2, 0, 0>>("Exp2");
		test.addOp<OpVoidAB<Log, 0, 1>>("Log");
		test.addOp<OpVoidAB<Log2, 0, 1>>("Log2");
		test.addOp<OpVoidAB<Log10, 0, 1>>("Log10");
		test.addOp<OpVoidAB<Sqrt, 0, 1>>("Sqrt");
		test.addOp<OpVoidAB<Cbrt, 0, 0>>("Cbrt");
		test.addOp<OpVoidAB<Floor, 0, 0>>("Floor");
		test.addOp<OpVoidAB<MinusFloor, 0, 0>>("MinusFloor");
		test.addOp<OpVoidAB<Ceil, 0, 0>>("Ceil");

		test.addOp<OpVoidABC<Max, 0, 0, 0>>("Max");
		test.addOp<OpVoidABC<Min, 0, 0, 0>>("Min");
	}
	{
		TestLinear test(maxSize, benchmarkSeconds, "realConstants");
		
		test.addOp<OpVoidABk<Assign, 0, 0>>("Assign");

		test.addOp<OpVoidABCk<Add, 0, 0, 0>>("Add");
		test.addOp<OpVoidABkC<Add, 0, 0, 0>>("Add2");
		test.addOp<OpVoidABCk<Sub, 0, 0, 0>>("Sub");
		test.addOp<OpVoidABkC<Sub, 0, 0, 0>>("Sub2");
		test.addOp<OpVoidABCk<Mul, 0, 0, 0>>("Mul");
		test.addOp<OpVoidABkC<Mul, 0, 0, 0>>("Mul2");
		test.addOp<OpVoidABCk<Div, 0, 0, 0>>("Div");
		test.addOp<OpVoidABkC<Div, 0, 0, 0>>("Div2");

		test.addOp<OpVoidABkC<Max, 0, 0, 0>>("Max");
		test.addOp<OpVoidABkC<Min, 0, 0, 0>>("Min");
		test.addOp<OpVoidABCk<Max, 0, 0, 0>>("Max2");
		test.addOp<OpVoidABCk<Min, 0, 0, 0>>("Min2");
	}
	{
		TestLinear test(maxSize, benchmarkSeconds, "complex");

		test.addOp<OpVoidAB<Assign, 2, 2>>("Assign");
		test.addOp<OpVoidAB<Neg, 2, 2>>("Neg");
		test.addOp<OpVoidAB<Conj, 2, 2>>("Conj");

		test.addOp<OpVoidABC<Add, 2, 2, 2>>("Add");
		test.addOp<OpVoidABC<Sub, 2, 2, 2>>("Sub");
		test.addOp<OpVoidABC<Mul, 2, 2, 2>>("Mul");
		test.addOp<OpVoidABC<Div, 2, 2, 2>>("Div");

		test.addOp<OpVoidAB<Exp, 2, 2>>("Exp");
		test.addOp<OpVoidAB<Log, 2, 2>>("Log");
		test.addOp<OpVoidAB<Log10, 2, 2>>("Log10");
	}
	{
		TestLinear test(maxSize, benchmarkSeconds, "complexConstants");

		test.addOp<OpVoidABk<Assign, 2, 2>>("Assign");

		test.addOp<OpVoidABCk<Add, 2, 2, 2>>("Add");
		test.addOp<OpVoidABkC<Add, 2, 2, 2>>("Add2");
		test.addOp<OpVoidABCk<Sub, 2, 2, 2>>("Sub");
		test.addOp<OpVoidABkC<Sub, 2, 2, 2>>("Sub2");
		test.addOp<OpVoidABCk<Mul, 2, 2, 2>>("Mul");
		test.addOp<OpVoidABkC<Mul, 2, 2, 2>>("Mul2");
		test.addOp<OpVoidABCk<Div, 2, 2, 2>>("Div");
		test.addOp<OpVoidABkC<Div, 2, 2, 2>>("Div2");
	}
	{
		TestLinear test(maxSize, benchmarkSeconds, "split");

		test.addOp<OpVoidAB<Assign, 3, 3>>("Assign");
		test.addOp<OpVoidAB<Neg, 3, 3>>("Neg");
		test.addOp<OpVoidAB<Conj, 3, 3>>("Conj");

		test.addOp<OpVoidABC<Add, 3, 3, 3>>("Add");
		test.addOp<OpVoidABC<Sub, 3, 3, 3>>("Sub");
		test.addOp<OpVoidABC<Mul, 3, 3, 3>>("Mul");
		test.addOp<OpVoidABC<Div, 3, 3, 3>>("Div");

		test.addOp<OpVoidAB<Exp, 3, 3>>("Exp");
		test.addOp<OpVoidAB<Log, 3, 3>>("Log");
		test.addOp<OpVoidAB<Log10, 3, 3>>("Log10");
	}
	{
		TestLinear test(maxSize, benchmarkSeconds, "splitConstants");

		test.addOp<OpVoidABk<Assign, 3, 3>>("Assign");

		test.addOp<OpVoidABCk<Add, 3, 3, 3>>("Add");
		test.addOp<OpVoidABkC<Add, 3, 3, 3>>("Add2");
		test.addOp<OpVoidABCk<Sub, 3, 3, 3>>("Sub");
		test.addOp<OpVoidABkC<Sub, 3, 3, 3>>("Sub2");
		test.addOp<OpVoidABCk<Mul, 3, 3, 3>>("Mul");
		test.addOp<OpVoidABkC<Mul, 3, 3, 3>>("Mul2");
		test.addOp<OpVoidABCk<Div, 3, 3, 3>>("Div");
		test.addOp<OpVoidABkC<Div, 3, 3, 3>>("Div2");
	}
	{
		TestLinear test(maxSize, benchmarkSeconds, "mixed");
		test.addOp<OpVoidAB<Assign, 2, 0>>("AssignCR");
		test.addOp<OpVoidAB<Neg, 2, 0>>("Neg");
		test.addOp<OpVoidAB<Neg, 3, 0>>("Neg-Split");
		test.addOp<OpVoidAB<Abs, 3, 2>>("AbsC-Split");
		test.addOp<OpVoidAB<Abs, 3, 3>>("AbsS-Split");
		test.addOp<OpVoidAB<Abs, 3, 0>>("AbsR-Split");
		
		test.addOp<OpVoidABC<Add, 2, 0, 0>>("AddCRR");
		test.addOp<OpVoidABC<Add, 2, 2, 0>>("AddCCR");
		test.addOp<OpVoidABC<Add, 2, 0, 2>>("AddCRC");
		
		test.addOp<OpVoidABC<Sub, 2, 0, 0>>("SubCRR");
		test.addOp<OpVoidABC<Sub, 2, 2, 0>>("SubCCR");
		test.addOp<OpVoidABC<Sub, 2, 0, 2>>("SubCRC");

		test.addOp<OpVoidABC<Mul, 2, 0, 0>>("MulCRR");
		test.addOp<OpVoidABC<Mul, 3, 0, 0>>("MulSRR");
		test.addOp<OpVoidABC<Mul, 2, 2, 0>>("MulCCR");
		test.addOp<OpVoidABC<Mul, 2, 0, 2>>("MulCRC");
		test.addOp<OpVoidABC<Mul, 3, 0, 3>>("MulSRS");
		test.addOp<OpVoidABC<Mul, 2, 0, 3>>("MulCRS");
		test.addOp<OpVoidABC<Mul, 3, 0, 2>>("MulSRC");

		test.addOp<OpVoidABC<Div, 2, 0, 0>>("DivCRR");
		test.addOp<OpVoidABC<Div, 2, 2, 0>>("DivCCR");
		test.addOp<OpVoidABC<Div, 2, 0, 2>>("DivCRC");
	}

	{
		TestLinear test(maxSize, benchmarkSeconds, "complex2real");
		test.addOp<OpVoidAB<Norm, 0, 2>>("Norm");
		test.addOp<OpVoidAB<Abs, 0, 2>>("Abs");
		test.addOp<OpVoidAB<SqrtNorm, 0, 2>>("SqrtNorm");
		test.addOp<OpVoidAB<Real, 0, 2>>("Real");
		test.addOp<OpVoidAB<Imag, 0, 2>>("Imag");
		test.addOp<OpVoidAB<Arg, 0, 2>>("Arg");
	}
	{
		TestLinear test(maxSize, benchmarkSeconds, "split2real");
		test.addOp<OpVoidAB<Norm, 0, 3>>("Norm");
		test.addOp<OpVoidAB<Abs, 0, 3>>("Abs");
		test.addOp<OpVoidAB<SqrtNorm, 0, 3>>("SqrtNorm");
		test.addOp<OpVoidAB<Real, 0, 3>>("Real");
		test.addOp<OpVoidAB<Imag, 0, 3>>("Imag");
		test.addOp<OpVoidAB<Arg, 0, 3>>("Arg");
	}
	{
		TestLinear test(maxSize, benchmarkSeconds, "realCompound");

		test.addOp<OpVoidAB<AbsNeg, 0, 0>>("AbsNeg");

		test.addOp<OpVoidABCD<MulAdd, 0, 0, 0, 0>>("MulAddR");
		test.addOp<OpVoidABCD<MulAdd2, 0, 0, 0, 0>>("MulAddR2");
		test.addOp<OpVoidABCD<AddMul, 0, 0, 0, 0>>("AddMulR");
		test.addOp<OpVoidABCD<AddMul2, 0, 0, 0, 0>>("AddMul2R");
		test.addOp<OpVoidABCD<MulSub, 0, 0, 0, 0>>("MulSubR");
		test.addOp<OpVoidABCD<MulSub2, 0, 0, 0, 0>>("MulSubR2");
		test.addOp<OpVoidABCD<SubMul, 0, 0, 0, 0>>("SubMulR");
		test.addOp<OpVoidABCD<SubMul2, 0, 0, 0, 0>>("SubMul2R");
		
		test.addOp<OpVoidABCDE<AddAddMul, 0, 0, 0, 0, 0>>("AddAddMul");
		test.addOp<OpVoidABCDE<AddSubMul, 0, 0, 0, 0, 0>>("AddSubMul");
		test.addOp<OpVoidABCDE<SubAddMul, 0, 0, 0, 0, 0>>("SubAddMul");
		test.addOp<OpVoidABCDE<SubSubMul, 0, 0, 0, 0, 0>>("SubSubMul");
		test.addOp<OpVoidABCDE<MulMulAdd, 0, 0, 0, 0, 0>>("MulMulAdd");
		test.addOp<OpVoidABCDE<MulMulSub, 0, 0, 0, 0, 0>>("MulMulSub");
	}
}
