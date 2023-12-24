#include<wasmtime.hh>
#include<iostream>
#include<cassert>
#include<cstdlib>
#include<variant>
#include<fstream>

static wasmtime::Result<std::monostate, wasmtime::Trap> hello_callback(wasmtime::Caller caller, wasmtime::Span<const wasmtime::Val> args, wasmtime::Span<wasmtime::Val> resultss) {
  std::cout << "Calling back..." << std::endl;
  std::cout << "Hello World!" << std::endl;
  return wasmtime::Result<std::monostate, wasmtime::Trap>(std::monostate());
}

int main(int argc, char *argv[]) {
  std::cout << "Initializing..." << std::endl;
  auto engine = new wasmtime::Engine();
  assert(engine != nullptr);

  auto store = new wasmtime::Store(*engine);
  assert(store != nullptr);

  auto context = new wasmtime::Store::Context(*store);
  assert(context != nullptr);

  // コマンドライン引数のチェック
  // -i または --input で入力ファイルを指定する
  if (argc < 3) {
    std::cerr << "ERROR: No input file specified." << std::endl;
    std::exit(1);
  }

  std::string input_file;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "-i" || std::string(argv[i]) == "--input") {
      if (i + 1 < argc) {
        input_file = argv[i + 1];
        break;
      }
    }
  }

  if (input_file.empty()) {
    std::cerr << "ERROR: No input file specified." << std::endl;
    std::exit(1);
  }

  // モジュールの読み込み
  std::cout << "Loading Wasm module..." << std::endl;

  //c++ライブラリを使ってファイルを読み込む
  std::ifstream ifs(input_file);
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  auto wasm_result = wasmtime::wat2wasm(content);
  if (!wasm_result) {
    auto error = wasm_result.err();
    auto message = error.message();
    std::cerr << "ERROR: " << message << std::endl;
    std::exit(1);
  }

  auto wasm = wasm_result.unwrap();

  auto module_result = wasmtime::Module::compile(*engine, wasm);
  if (!module_result) {
    auto error = module_result.err();
    auto i32_exit = error.i32_exit();
    auto message = error.message();
    auto trace = error.trace();

    std::cerr << "ERROR: " << message << std::endl;
    for(auto it=trace.begin(); it!=trace.end(); ++it) {
      std::cerr << "  at " << *it->func_name() << " (" << *it->module_name() << ")" << std::endl;
    }
    std::exit(1);
  }

  auto module = module_result.unwrap();

  // 組み込み関数の登録
  std::cout << "Registering functions..." << std::endl;
  auto hello_callback_func_type = new wasmtime::FuncType({}, {});
  assert(hello_callback_func_type != nullptr);

  auto hello_callback_func = new wasmtime::Func(*context, *hello_callback_func_type, hello_callback);
  assert(hello_callback_func != nullptr);

  // モジュールのインスタンス化
  std::cout << "Instantiating module..." << std::endl;
  auto instance_result = wasmtime::Instance::create(*context, module, {*hello_callback_func});
  if (!instance_result) {
    auto error = instance_result.err();
    auto message = error.message();
    std::cerr << "ERROR: " << message << std::endl;
    std::exit(1);
  }

  auto instance = instance_result.unwrap();

  // エクスポートされた関数の呼び出し
  std::cout << "Calling exported function..." << std::endl;
  auto run_func = instance.get(*context, "run");
  if (!run_func) {
    std::cerr << "ERROR: No exported function 'run' found." << std::endl;
    std::exit(1);
  }

  auto run_result = std::get<wasmtime::Func>(run_func.value());
  auto run_result_result = run_result.call(*context, {});
  if (!run_result_result) {
    auto error = run_result_result.err();
    auto message = error.message();
    std::cerr << "ERROR: " << message << std::endl;
    std::exit(1);
  }

  std::cout << "Done." << std::endl;

  return 0;
}
