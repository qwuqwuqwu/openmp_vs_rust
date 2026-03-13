import pandas as pd

INPUT_FILE = "results_overhead.csv"
OUTPUT_FILE = "summary_overhead.csv"

df = pd.read_csv(INPUT_FILE)

metrics = [
    "region_avg_sec",
    "barrier_avg_sec",
    "atomic_avg_sec_per_increment"
]

summary = df.groupby("threads")[metrics].agg(
    ["mean", "std", "min", "max"]
)

summary.columns = [
    "_".join(col).strip()
    for col in summary.columns.values
]

summary.reset_index(inplace=True)

summary.to_csv(OUTPUT_FILE, index=False)

print("Summary saved to:", OUTPUT_FILE)
print()
print(summary)