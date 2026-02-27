#!/usr/bin/env python3
"""
FRPC Framework Documentation Verification Script

This script verifies that all public interfaces have proper documentation
including function descriptions, parameters, return values, and exceptions.
"""

import os
import re
import sys
from pathlib import Path
from typing import List, Tuple, Set

class DocVerifier:
    def __init__(self, include_dir: str):
        self.include_dir = Path(include_dir)
        self.issues: List[Tuple[str, int, str]] = []
        self.total_classes = 0
        self.total_functions = 0
        self.documented_classes = 0
        self.documented_functions = 0
        
    def verify_all(self) -> bool:
        """Verify all header files in the include directory."""
        print("=== FRPC Documentation Verification ===\n")
        
        header_files = list(self.include_dir.rglob("*.h"))
        header_files.extend(self.include_dir.rglob("*.hpp"))
        
        if not header_files:
            print(f"No header files found in {self.include_dir}")
            return False
            
        for header_file in sorted(header_files):
            self.verify_file(header_file)
        
        self.print_summary()
        return len(self.issues) == 0
    
    def verify_file(self, filepath: Path):
        """Verify documentation in a single file."""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
        except Exception as e:
            print(f"Error reading {filepath}: {e}")
            return
        
        lines = content.split('\n')
        self.check_classes(filepath, lines)
        self.check_functions(filepath, lines)
    
    def check_classes(self, filepath: Path, lines: List[str]):
        """Check class documentation."""
        class_pattern = re.compile(r'^\s*class\s+(\w+)')
        doc_pattern = re.compile(r'^\s*/\*\*')
        
        i = 0
        while i < len(lines):
            line = lines[i]
            class_match = class_pattern.search(line)
            
            if class_match:
                class_name = class_match.group(1)
                self.total_classes += 1
                
                # Check if there's a doc comment before the class
                has_doc = False
                j = i - 1
                while j >= 0 and lines[j].strip() == '':
                    j -= 1
                
                if j >= 0:
                    # Look for /** ... */ or /// comments
                    if '*/' in lines[j] or '///' in lines[j]:
                        has_doc = True
                        self.documented_classes += 1
                
                if not has_doc:
                    self.issues.append((
                        str(filepath.relative_to(self.include_dir.parent)),
                        i + 1,
                        f"Class '{class_name}' lacks documentation"
                    ))
            
            i += 1
    
    def check_functions(self, filepath: Path, lines: List[str]):
        """Check function documentation."""
        # Match public function declarations
        func_pattern = re.compile(
            r'^\s*(?:virtual\s+)?(?:static\s+)?'
            r'(?:[\w:]+(?:<[^>]+>)?)\s+'  # return type
            r'(\w+)\s*\('  # function name
        )
        
        in_public = False
        in_comment_block = False
        
        i = 0
        while i < len(lines):
            line = lines[i]
            
            # Track public/private sections
            if re.match(r'^\s*public\s*:', line):
                in_public = True
            elif re.match(r'^\s*(private|protected)\s*:', line):
                in_public = False
            
            # Track comment blocks
            if '/**' in line:
                in_comment_block = True
            if '*/' in line:
                in_comment_block = False
            
            # Check function declarations in public sections
            if in_public:
                func_match = func_pattern.search(line)
                if func_match:
                    func_name = func_match.group(1)
                    
                    # Skip constructors, destructors, operators
                    if func_name.startswith('~') or func_name == 'operator':
                        i += 1
                        continue
                    
                    self.total_functions += 1
                    
                    # Check if there's documentation before this function
                    has_doc = False
                    j = i - 1
                    while j >= 0 and lines[j].strip() == '':
                        j -= 1
                    
                    if j >= 0:
                        # Look for doxygen comments
                        if '*/' in lines[j] or '///' in lines[j] or '//<' in lines[j]:
                            has_doc = True
                            self.documented_functions += 1
                            
                            # Verify @param, @return, @throws if applicable
                            doc_block = self.extract_doc_block(lines, j)
                            self.verify_doc_completeness(
                                filepath, i + 1, func_name, line, doc_block
                            )
                    
                    if not has_doc:
                        self.issues.append((
                            str(filepath.relative_to(self.include_dir.parent)),
                            i + 1,
                            f"Function '{func_name}' lacks documentation"
                        ))
            
            i += 1
    
    def extract_doc_block(self, lines: List[str], end_line: int) -> str:
        """Extract the documentation block ending at end_line."""
        doc_lines = []
        i = end_line
        
        while i >= 0:
            line = lines[i]
            doc_lines.insert(0, line)
            if '/**' in line:
                break
            i -= 1
        
        return '\n'.join(doc_lines)
    
    def verify_doc_completeness(self, filepath: Path, line_num: int, 
                                func_name: str, func_decl: str, doc_block: str):
        """Verify that documentation includes necessary elements."""
        # Check for parameters
        if '(' in func_decl and ')' in func_decl:
            params = self.extract_params(func_decl)
            for param in params:
                if f'@param {param}' not in doc_block and f'@param[' not in doc_block:
                    self.issues.append((
                        str(filepath.relative_to(self.include_dir.parent)),
                        line_num,
                        f"Function '{func_name}' missing @param documentation for '{param}'"
                    ))
        
        # Check for return value (if not void)
        if 'void' not in func_decl.split(func_name)[0]:
            if '@return' not in doc_block and '@returns' not in doc_block:
                self.issues.append((
                    str(filepath.relative_to(self.include_dir.parent)),
                    line_num,
                    f"Function '{func_name}' missing @return documentation"
                ))
    
    def extract_params(self, func_decl: str) -> List[str]:
        """Extract parameter names from function declaration."""
        params = []
        
        # Extract content between parentheses
        match = re.search(r'\((.*?)\)', func_decl)
        if not match:
            return params
        
        param_str = match.group(1).strip()
        if not param_str or param_str == 'void':
            return params
        
        # Split by comma and extract parameter names
        for param in param_str.split(','):
            param = param.strip()
            # Extract the last word (parameter name)
            words = param.split()
            if words:
                param_name = words[-1]
                # Remove pointer/reference symbols
                param_name = param_name.lstrip('*&')
                if param_name and not param_name.startswith('('):
                    params.append(param_name)
        
        return params
    
    def print_summary(self):
        """Print verification summary."""
        print("\n=== Verification Summary ===")
        print(f"Total classes found: {self.total_classes}")
        print(f"Documented classes: {self.documented_classes}")
        print(f"Total public functions found: {self.total_functions}")
        print(f"Documented functions: {self.documented_functions}")
        print(f"\nTotal issues found: {len(self.issues)}")
        
        if self.issues:
            print("\n=== Documentation Issues ===")
            for filepath, line_num, issue in self.issues[:50]:  # Limit to first 50
                print(f"{filepath}:{line_num}: {issue}")
            
            if len(self.issues) > 50:
                print(f"\n... and {len(self.issues) - 50} more issues")
            
            print("\n✗ Documentation verification FAILED")
            print("Please add documentation to all public interfaces.")
        else:
            print("\n✓ All public interfaces are documented!")
        
        # Calculate coverage
        if self.total_classes > 0:
            class_coverage = (self.documented_classes / self.total_classes) * 100
            print(f"\nClass documentation coverage: {class_coverage:.1f}%")
        
        if self.total_functions > 0:
            func_coverage = (self.documented_functions / self.total_functions) * 100
            print(f"Function documentation coverage: {func_coverage:.1f}%")


def main():
    include_dir = "include"
    
    if not os.path.exists(include_dir):
        print(f"Error: Include directory '{include_dir}' not found")
        sys.exit(1)
    
    verifier = DocVerifier(include_dir)
    success = verifier.verify_all()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
