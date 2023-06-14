const std = @import("std");

const lax_cflags = [_][]const u8{"-std=c11"};

const strict_cflags = lax_cflags ++ [_][]const u8{
    "-Werror",
    "-Wall",
    "-Wextra",
    "-Wunused-parameter",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lexer_gen = b.addSystemCommand(&.{
        "flex",
        "--hex", //  use hexadecimal numbers instead of octal in debug outputs
        "--8bit", // generate 8-bit scanner
        "--batch", // generate batch scanner
        "--yylineno", //  track line count in yylineno
        // "--prefix=apigen_parser_", // custom prefix instead of yy
        "--reentrant", // generate a reentrant C scanner
        "--nounistd", //  do not include <unistd.h>
        "--bison-locations", // include yylloc support.
        "--bison-bridge", // scanner for bison pure parser.
    });
    const lexer_c_source = lexer_gen.addPrefixedOutputFileArg("--outfile=", "lexer.yy.c");

    lexer_gen.addArg("--header-file=zig-cache/lexer.yy.h"); // HACK UNTIL INCLUDES ARE RESOLVED
    // const lexer_h_source = lexer_gen.addPrefixedOutputFileArg("--header-file=", "lexer.yy.h");
    // _ = lexer_h_source;

    lexer_gen.addFileSourceArg(.{ .path = "src/parser/lexer.l" });

    const parser_gen = b.addSystemCommand(&.{
        "bison",
        "--language=c", // specify the output programming language
        "--locations", // enable location support
        // "--file-prefix=PREFIX", // specify a PREFIX for output files
    });
    const parser_c_source = parser_gen.addPrefixedOutputFileArg("--output=", "parser.yy.c");

    parser_gen.addArg("--header=zig-cache/parser.yy.h"); // HACK UNTIL INCLUDES ARE RESOLVED
    // const lexer_h_source = lexer_gen.addPrefixedOutputFileArg("--header-file=", "parser.yy.h");
    // _ = lexer_h_source;

    parser_gen.addFileSourceArg(.{ .path = "src/parser/parser.y" });

    // the "flex" output depends on the header that is generated by
    // the "bison" step
    lexer_gen.step.dependOn(&parser_gen.step);

    const exe = b.addExecutable(.{
        .name = "apidef",
        .target = target,
        .optimize = optimize,
    });

    exe.addIncludePath("zig-cache"); // HACK UNTIL INCLUDES ARE RESOLVED
    exe.addIncludePath("src"); // HACK UNTIL INCLUDES ARE RESOLVED

    exe.linkLibC();
    exe.addIncludePath("include");
    exe.addCSourceFiles(
        &.{
            "src/apigen.c",
            "src/io.c",
            "src/parser/parser.c",
            "src/gen/c_cpp.c",
            "src/gen/rust.c",
            "src/gen/zig.c",
        },
        &strict_cflags,
    );
    exe.addCSourceFileSource(.{ .source = lexer_c_source, .args = &lax_cflags });
    exe.addCSourceFileSource(.{ .source = parser_c_source, .args = &lax_cflags });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);

    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
