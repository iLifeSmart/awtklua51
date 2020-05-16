# awtklua51

fork from awtk-lua [https://github.com/zlgopen/awtk-lua]

first build awtk  
https://github.com/zlgopen/awtk
cd awtk
scons

cd ../awtklua51
scons

#on macos
cp bin/libopenawtk.dylib openawtk.so

export DYLD_LIBRARY_PATH=../awtk/bin/

run:

luajit

--lua code for init
require'openawtk'

LCD_W=480
LCD_H=480

function on_click(ctx, evt) 
  print('on_click');
  return Ret.OK;
end

function application_init()
	local win=Window.open("main")
	win:layout();
end

Global.init(LCD_W,LCD_H,1,'DemoLua','/xxxx/xxxx/prjdir')
AssetsManager.instance():set_theme("default")
AssetsManager.instance():preload(AssetType.FONT, "default");
AssetsManager.instance():preload(AssetType.STYLE, "default");
Global.init_assets();
application_init()

Global.run()


