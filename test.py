import configparser
import subprocess
import random

PROGRAM = "./program"
FILE = "./example-4.5.ini"

config = configparser.ConfigParser()
config.read(FILE)

print("= Retrival Testing")

total_section = len(config.sections())

text_values = []
num_values = []

for i, section in enumerate(config.sections()):
    for key in config[section]:
        result = subprocess.run(
            [PROGRAM, FILE, section + "." + key],
            capture_output=True,
            text=True,
        )

        if result.returncode != 0:
            print(f"Error at value: {section}.{key}")
            print(f"Stdout: {result.stdout}")
            print(f"Stderr: {result.stderr}")
            exit(1)

        value = result.stdout.rstrip()
        if value != config[section][key]:
            print(f"Value mismatch {section}.{key}, expected: {config[section][key]}, got: {value}.")
            exit(1)

        try:
            int(value)
            num_values.append((section, key))
        except ValueError:
            text_values.append((section, key))

    print(f"Finished section {i + 1}/{total_section}")


N_TESTS = 200

print("= Text Expressions Testing")

for i in range(N_TESTS):
    lhs, rhs = random.choices(text_values, k=2)
    result = subprocess.run(
        [PROGRAM, FILE, "expression", f"{lhs[0]}.{lhs[1]} + {rhs[0]}.{rhs[1]}"],
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        print(f"Error at expression: {lhs[0]}.{lhs[1]} + {rhs[0]}.{rhs[1]}")
        print(f"Stdout: {result.stdout}")
        print(f"Stderr: {result.stderr}")
        exit(1)

    expected = config[lhs[0]][lhs[1]] + config[rhs[0]][rhs[1]]
    if expected != result.stdout.rstrip():
        print(f"Value mismatch {lhs[0]}.{lhs[1]} + {rhs[0]}.{rhs[1]}")
        print(f"Expected: {expected}\n")
        print(f"Got: {result.stdout.rstrip()}.")
        exit(1)

    if (i + 1) % 10 == 0:
        print(f"{i + 1}/{N_TESTS} completed.")


print("= Number Expressions Testing")

for i in range(N_TESTS):
    lhs, rhs = random.choices(num_values, k=2)
    lhs_v = int(config[lhs[0]][lhs[1]])
    rhs_v = int(config[rhs[0]][rhs[1]])
    for op in "+-*/":
        if op == "/" and rhs_v == 0:
            continue

        result = subprocess.run(
            [PROGRAM, FILE, "expression", f"{lhs[0]}.{lhs[1]} {op} {rhs[0]}.{rhs[1]}"],
            capture_output=True,
            text=True,
        )

        if result.returncode != 0:
            print(f"Error at expression: {lhs[0]}.{lhs[1]} {op} {rhs[0]}.{rhs[1]}")
            print(f"Stdout: {result.stdout}")
            print(f"Stderr: {result.stderr}")
            exit(1)

        match op:
            case '+':
                expected = lhs_v + rhs_v
            case '-':
                expected = lhs_v - rhs_v
            case '*':
                expected = lhs_v * rhs_v
            case '/':
                expected = lhs_v // rhs_v
            case _:
                raise RuntimeError("Unknown Operator")

        if str(expected) != result.stdout.rstrip():
            print(f"Value mismatch {lhs[0]}.{lhs[1]} {op} {rhs[0]}.{rhs[1]}")
            print(f"Expected: {expected}\n")
            print(f"Got: {result.stdout.rstrip()}")
            exit(1)

    if (i + 1) % 10 == 0:
        print(f"{i + 1}/{N_TESTS} completed.")

