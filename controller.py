import ctypes
from transformers import pipeline

# -------------------------------------------------
# LOAD C MEMORY MODULE
# -------------------------------------------------
memory = ctypes.CDLL("./memory.so")

memory.process_message.argtypes = [ctypes.c_char_p]
memory.export_context.argtypes = [ctypes.c_char_p]
memory.export_full_state.argtypes = [ctypes.c_char_p]

# -------------------------------------------------
# LOAD LOCAL LLM (VERY SMALL, FAST)
# -------------------------------------------------
print("Loading local LLM (first time may take a few seconds)...")

llm = pipeline(
    "text2text-generation",
    model="google/flan-t5-small",
    device=-1   # CPU
)

print("LLM loaded successfully.\n")

# -------------------------------------------------
# MAIN LOOP
# -------------------------------------------------
print("Talk to the LLM (type 'exit' to stop)\n")

while True:
    user_input = input("You: ")
    if user_input.lower() == "exit":
        break

    # 1️⃣ Send USER input to C memory
    memory.process_message(user_input.encode())

    # 2️⃣ Export compressed memory (LTQ)
    memory.export_full_state(b"context.txt")

    # 3️⃣ Read compressed memory
    with open("context.txt", "r") as f:
        memory_context = f.read()

    # 🔍 SHOW MEMORY (THIS IS THE KEY PART)
    print("\n--- COMPRESSED MEMORY (LTQ) ---")
    if memory_context.strip():
        print(memory_context.strip())
    else:
        print("[EMPTY]")
    print("------------------------------\n")

    # 4️⃣ Build prompt for LLM
    prompt = f"""
You are an assistant with access to compressed long-term memory.

Memory:
{memory_context}

User question:
{user_input}
"""

    # 5️⃣ Run local LLM
    result = llm(prompt, max_new_tokens=100)
    reply = result[0]["generated_text"]

    # 6️⃣ Show LLM response
    print("LLM:", reply, "\n")

   