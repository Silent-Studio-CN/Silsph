// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// sp_rust_demo.rs — Silsph 软渲染器 Rust FFI 绑定（LoadLibrary 动态加载，零 crate 依赖）
// 编译: rustc -O sp_rust_demo.rs -o sp_rust_demo.exe
// 验证: 渲染 cube 场景并与 C 黄金图 (golden/cube.raw) 逐字节比对
use std::ffi::CString;
use std::os::raw::{c_char, c_float, c_int, c_void};

#[link(name = "kernel32")]
extern "system" {
    fn LoadLibraryA(name: *const c_char) -> *mut c_void;
    fn GetProcAddress(module: *mut c_void, name: *const c_char) -> *mut c_void;
}

macro_rules! sym {
    ($dll:expr, $t:ty, $n:literal) => {
        std::mem::transmute::<*mut c_void, $t>(GetProcAddress($dll, CString::new($n).unwrap().as_ptr()))
    };
}

type F0 = unsafe extern "system" fn();
type F1i = unsafe extern "system" fn(c_int);
type F1u = unsafe extern "system" fn(u32);
type F2i = unsafe extern "system" fn(c_int, c_int) -> c_int;
type F3f = unsafe extern "system" fn(c_float, c_float, c_float);
type F4i = unsafe extern "system" fn(c_int, c_int, c_int, c_int);
type F4f = unsafe extern "system" fn(c_float, c_float, c_float, c_float);
type F9f = unsafe extern "system" fn(c_float, c_float, c_float, c_float, c_float, c_float, c_float, c_float, c_float);
type FPix = unsafe extern "system" fn(*mut c_int, *mut c_int) -> *mut u8;

const SP_COLOR: u32 = 0x4000;
const SP_DEPTH: u32 = 0x0100;
const SP_TRIANGLES: c_int = 0x0004;
const SP_LINES: c_int = 0x0001;
const SP_MODELVIEW: c_int = 0x1700;
const SP_PROJECTION: c_int = 0x1701;

fn main() {
    unsafe {
        let dll = LoadLibraryA(CString::new("silsph_soft.dll").unwrap().as_ptr());
        assert!(!dll.is_null(), "silsph_soft.dll 加载失败");

        let sp_create: F2i = sym!(dll, F2i, "sp_create");
        let sp_viewport: F4i = sym!(dll, F4i, "sp_viewport");
        let sp_clear_color: F4f = sym!(dll, F4f, "sp_clear_color");
        let sp_clear: F1u = sym!(dll, F1u, "sp_clear");
        let sp_matrix_mode: F1i = sym!(dll, F1i, "sp_matrix_mode");
        let sp_load_identity: F0 = sym!(dll, F0, "sp_load_identity");
        let sp_perspective: F4f = sym!(dll, F4f, "sp_perspective");
        let sp_look_at: F9f = sym!(dll, F9f, "sp_look_at");
        let sp_rotate: F4f = sym!(dll, F4f, "sp_rotate");
        let sp_begin: F1i = sym!(dll, F1i, "sp_begin");
        let sp_color3f: F3f = sym!(dll, F3f, "sp_color3f");
        let sp_vertex3f: F3f = sym!(dll, F3f, "sp_vertex3f");
        let sp_end: F0 = sym!(dll, F0, "sp_end");
        let sp_blend: F1i = sym!(dll, F1i, "sp_blend");
        let sp_pixels: FPix = sym!(dll, FPix, "sp_pixels");
        let sp_destroy: F0 = sym!(dll, F0, "sp_destroy");

        let w = 480i32;
        let h = 320i32;
        assert!(sp_create(w, h) == 1);
        sp_viewport(0, 0, w, h);
        sp_clear_color(0.063, 0.075, 0.102, 1.0);
        sp_clear(SP_COLOR | SP_DEPTH);

        // ---- cube 场景（与 regress.c / pysilsph.py 同参数，angle=0） ----
        sp_matrix_mode(SP_PROJECTION);
        sp_load_identity();
        sp_perspective(55.0, w as f32 / h as f32, 0.1, 100.0);
        sp_matrix_mode(SP_MODELVIEW);
        sp_load_identity();
        sp_look_at(0.0, 8.5, 17.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0); /* angle=0: sin0=0, cos0=1 */
        sp_rotate(0.0, 0.0, 1.0, 0.0);
        sp_rotate(0.0, 1.0, 0.0, 0.0);

        let faces: [[f32; 6]; 6] = [
            [0.0, 0.0, -1.0, 1.0, 0.62, 0.25],
            [0.0, 0.0, 1.0, 0.31, 1.0, 0.99],
            [-1.0, 0.0, 0.0, 0.44, 0.69, 1.0],
            [1.0, 0.0, 0.0, 1.0, 0.44, 0.69],
            [0.0, -1.0, 0.0, 0.61, 1.0, 0.44],
            [0.0, 1.0, 0.0, 1.0, 0.85, 0.44],
        ];
        for f in faces.iter() {
            let (nx, ny, nz) = (f[0], f[1], f[2]);
            let tx: [f32; 3] = if nz != 0.0 {
                [1.0, 0.0, 0.0]
            } else if ny != 0.0 {
                [1.0, 0.0, 0.0]
            } else {
                [0.0, 0.0, 1.0]
            };
            let ty = [
                ny * tx[2] - nz * tx[1],
                nz * tx[0] - nx * tx[2],
                nx * tx[1] - ny * tx[0],
            ];
            sp_color3f(f[3], f[4], f[5]);
            sp_begin(SP_TRIANGLES);
            for (k0, k1, k2) in [(0usize, 2usize, 1usize), (0usize, 3usize, 2usize)] {
                for k in [k0, k1, k2] {
                    let sx = if k == 1 || k == 2 { 1.0 } else { -1.0 };
                    let sy = if k == 2 || k == 3 { 1.0 } else { -1.0 };
                    sp_vertex3f(
                        nx + tx[0] * sx + ty[0] * sy,
                        ny + tx[1] * sx + ty[1] * sy,
                        nz + tx[2] * sx + ty[2] * sy,
                    );
                }
            }
            sp_end();
        }
        // 半透明板
        sp_blend(1);
        sp_color3f(1.0, 1.0, 1.0);
        sp_begin(SP_TRIANGLES);
        sp_vertex3f(-1.5, -1.5, 2.5);
        sp_vertex3f(1.5, -1.5, 2.5);
        sp_vertex3f(1.5, 1.5, 2.5);
        sp_vertex3f(-1.5, -1.5, 2.5);
        sp_vertex3f(1.5, 1.5, 2.5);
        sp_vertex3f(-1.5, 1.5, 2.5);
        sp_end();
        sp_blend(0);
        // 网格
        sp_color3f(0.35, 0.40, 0.55);
        sp_begin(SP_LINES);
        let mut x = -8.0f32;
        while x <= 8.0 {
            sp_vertex3f(x, 0.0, -8.0);
            sp_vertex3f(x, 0.0, 8.0);
            x += 1.0;
        }
        let mut z = -8.0f32;
        while z <= 8.0 {
            sp_vertex3f(-8.0, 0.0, z);
            sp_vertex3f(8.0, 0.0, z);
            z += 1.0;
        }
        sp_end();

        // ---- 像素与黄金图比对 ----
        let mut wv = w;
        let mut hv = h;
        let px = sp_pixels(&mut wv, &mut hv);
        let data = std::slice::from_raw_parts(px, (wv * hv * 4) as usize);
        let golden = std::fs::read("golden/cube.raw").expect("golden/cube.raw 不存在（先运行 regress.exe --gen）");
        let diff = data
            .iter()
            .zip(golden.iter())
            .filter(|(a, b)| a != b)
            .count();
        println!(
            "Rust 渲染 {}x{}, 与 C 黄金图差异字节 = {} / {}",
            wv,
            hv,
            diff,
            golden.len()
        );
        let ok = diff == 0 && data.len() == golden.len();
        println!("{}", if ok { "ALL PASS" } else { "FAILED" });
        sp_destroy();
        std::process::exit(if ok { 0 } else { 1 });
    }
}
