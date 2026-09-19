# Weekday LLD Learn

AlgoMaster-style reading UI for the multi-language `Interview_problems` in this repo.

## Run

From the **repo root** (so language source files are fetchable):

```bash
python -m http.server 8877
```

Open [http://localhost:8877/lld-learn/](http://localhost:8877/lld-learn/).

Serving only the `lld-learn` folder will break the code viewer (`../Java/...` cannot escape that directory).

## Refresh problem data

After editing source headers under `*/Interview_problems`:

```bash
python lld-learn/_generate_data.py
```

## What’s included

- Dark curriculum sidebar + centered reading column
- Entity map + UML-style class diagram per problem
- VS Code–style editor: language tabs, Run / Copy / Reset, terminal, status bar
- Five languages loaded from `Interview_problems`

JavaScript Run executes in the browser (no network). Other languages use Wandbox. The old public Piston API now returns HTTP 401 without a key.
