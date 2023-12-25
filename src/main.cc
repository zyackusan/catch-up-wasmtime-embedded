#include <wasmtime.hh>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <variant>
#include <fstream>
#include <boost/program_options.hpp>

static wasmtime::Result<std::monostate, wasmtime::Trap> hello_callback(wasmtime::Caller caller, wasmtime::Span<const wasmtime::Val> args, wasmtime::Span<wasmtime::Val> resultss)
{
  std::cout << "Calling back..." << std::endl;
  std::cout << "Hello World!" << std::endl;
  return wasmtime::Result<std::monostate, wasmtime::Trap>(std::monostate());
}

int main(int argc, char *argv[])
{
  // コマンドライン引数のチェック
  // boost::program_options を使ってコマンドライン引数をパースする
  // https://www.boost.org/doc/libs/1_75_0/doc/html/program_options.html
  // -i または --input で入力ファイルを指定する
  // -t または --wot で WebAssembly Text Format であることを指定する
  // -h または --help でヘルプを表示する
  std::string input_file;
  bool is_wat = false;

  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce help message")("input,i", po::value<std::string>(&input_file), "input file")("wat,t", po::bool_switch(&is_wat), "input file is WebAssembly Text Format (wat)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // ヘルプを表示する
    if (vm.count("help"))
    {
      std::cout << desc << std::endl;
      std::exit(0);
    }

    // 入力ファイルが指定されていない
    if (!vm.count("input"))
    {
      std::cerr << "ERROR: No input file specified." << std::endl;
      std::exit(1);
    }

    // 入力ファイルが存在しない
    std::ifstream ifs(input_file);
    if (!ifs.is_open())
    {
      std::cerr << "ERROR: Input file not found." << std::endl;
      std::exit(1);
    }
  }
  catch (std::exception &e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl;
    std::exit(1);
  }

  // エンジンの初期化
  auto engine = new wasmtime::Engine();
  assert(engine != nullptr);

  auto store = new wasmtime::Store(*engine);
  assert(store != nullptr);

  auto context = new wasmtime::Store::Context(*store);
  assert(context != nullptr);

  // WASIの設定
  {
    wasmtime::WasiConfig wasi_config;
    wasi_config.inherit_stdout();
    wasi_config.inherit_stderr();
    wasi_config.inherit_stdin();
    wasi_config.argv({});
    wasi_config.env({});
    auto set_wasi_result = context->set_wasi(std::move(wasi_config));
    if (!set_wasi_result)
    {
      auto error = set_wasi_result.err();
      auto message = error.message();
      std::cerr << "ERROR: " << message << std::endl;
      std::exit(1);
    }
  }

  // リンカーの初期化
  wasmtime::Linker linker(*engine);
  {
    // WASIのリンク
    {
      auto linker_define_wasi_result = linker.define_wasi();
      if (!linker_define_wasi_result)
      {
        auto error = linker_define_wasi_result.err();
        auto message = error.message();
        std::cerr << "ERROR: " << message << std::endl;
        std::exit(1);
      }
    }

    // 組み込み関数のリンク
    {
      auto linker_define_result = linker.define(
          *context,
          "custom_embedded",
          "hello",
          {
              wasmtime::Func(
                  *context,
                  wasmtime::FuncType({}, {}),
                  hello_callback),
          });

      if (!linker_define_result)
      {
        auto error = linker_define_result.err();
        auto message = error.message();
        std::cerr << "ERROR: " << message << std::endl;
        std::exit(1);
      }
    }

    // モジュールのリンク
    {
      std::vector<uint8_t> wasm;
      if (is_wat)
      {
        std::ifstream ifs(input_file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        auto wasm_result = wasmtime::wat2wasm(content);
        if (!wasm_result)
        {
          auto error = wasm_result.err();
          auto message = error.message();
          std::cerr << "ERROR: " << message << std::endl;
          std::exit(1);
        }

        wasm = wasm_result.unwrap();
      }
      else
      {
        std::ifstream ifs(input_file, std::ios::binary);
        std::vector<uint8_t> content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        wasm = content;
      }

      auto module_result = wasmtime::Module::compile(*engine, wasm);
      if (!module_result)
      {
        auto error = module_result.err();
        auto i32_exit = error.i32_exit();
        auto message = error.message();
        auto trace = error.trace();

        std::cerr << "ERROR: " << message << std::endl;
        for (auto it = trace.begin(); it != trace.end(); ++it)
        {
          std::cerr << "  at " << *it->func_name() << " (" << *it->module_name() << ")" << std::endl;
        }
        std::exit(1);
      }

      auto module = module_result.unwrap();

      auto wasi_linker_result = linker.module(*context, "", module);
      if (!wasi_linker_result)
      {
        auto error = wasi_linker_result.err();
        auto message = error.message();
        std::cerr << "ERROR: " << message << std::endl;
        std::exit(1);
      }
    }
  }

  // WASIのデフォルト関数を取得して実行する
  {
    auto wasi_default_result = linker.get_default(*context, "");
    if (!wasi_default_result)
    {
      auto error = wasi_default_result.err();
      auto message = error.message();
      std::cerr << "ERROR: " << message << std::endl;
      std::exit(1);
    }

    auto wasi_default = wasi_default_result.unwrap();
    auto wasi_default_call_result = wasi_default.call(*context, {});
    if (!wasi_default_call_result)
    {
      auto error = wasi_default_call_result.err();
      auto message = error.message();
      std::cerr << "WARN: " << message << std::endl;
    }

    std::cout << "Done." << std::endl;
  }

  return 0;
}
