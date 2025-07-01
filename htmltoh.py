#!/usr/bin/env python3
import argparse
import os

def html_to_c_header(html_path: str, header_path: str = None):

    # Define o nome do header de saída
    if header_path is None:
        base = os.path.splitext(html_path)[0]
        header_path = base + ".h"

    base_name = os.path.basename(os.path.splitext(header_path)[0])
    var_name = f"{base_name}_BODY"
    guard    = base_name.upper().replace('.', '_') + "_H"

    # Lê e processa linha a linha
    with open(html_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Gera o header
    with open(header_path, 'w', encoding='utf-8') as out:
        out.write(f"#ifndef {guard}\n")
        out.write(f"#define {guard}\n\n")
        out.write(f"#define {var_name.upper()} \\ \n")

        for i,line in enumerate(lines):
            line = line.rstrip('\r\n')                     # remove \n ou \r\n
            line = line.replace('\\', '\\\\')              # escapa barra invertida
            line = line.replace('"', '\\"')                # escapa aspas duplas
            bn = '\\'
            out.write(f"    \"{line}\" {bn if i < len(lines) - 1 else ''}\n")                 # escreve a linha como literal
        
        out.write("\n")
        out.write(f"#endif /* {guard} */\n")

    print(f"✅ Header gerado: {header_path}")

def main():
    parser = argparse.ArgumentParser(description="Converte HTML em header C.")
    parser.add_argument("html_file", help="arquivo .html de entrada")
    parser.add_argument("-o", "--output", help="arquivo .h de saída")
    args = parser.parse_args()

    html_to_c_header(args.html_file, args.output)

if __name__ == "__main__":
    main()
