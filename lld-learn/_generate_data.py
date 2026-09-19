"""Generate lld-learn/data/problems.js from Interview_problems source headers."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = Path(__file__).resolve().parent / "data"

LANGS = {
    "java": ("Java/Interview_problems", ".java"),
    "javascript": ("JavaScript/Interview_problems", ".js"),
    "python": ("Python/Interview_problems", ".py"),
    "cpp": ("C++/Interview_problems", ".cpp"),
    "golang": ("Golang/Interview_problems", ".go"),
}

META = {
    "Amazon_Locker_demo": {
        "slug": "amazon-locker",
        "title": "Design Amazon Locker",
        "category": "ecommerce",
        "priority": "High",
        "difficulty": "medium",
        "readMins": 25,
    },
    "Chess_Game": {
        "slug": "chess-game",
        "title": "Design Chess Game",
        "category": "games",
        "priority": "High",
        "difficulty": "hard",
        "readMins": 35,
    },
    "Elevator_System_demo": {
        "slug": "elevator-system",
        "title": "Design Elevator System",
        "category": "states",
        "priority": "High",
        "difficulty": "hard",
        "readMins": 30,
    },
    "File_System_demo": {
        "slug": "file-system",
        "title": "Design In-Memory File System",
        "category": "infra",
        "priority": "Medium",
        "difficulty": "medium",
        "readMins": 20,
    },
    "Kafka_demo": {
        "slug": "kafka",
        "title": "Design Pub-Sub (Kafka-like)",
        "category": "messaging",
        "priority": "High",
        "difficulty": "hard",
        "readMins": 30,
    },
    "LLD_questions": {
        "slug": "notification-system",
        "title": "Design Notification System",
        "category": "messaging",
        "priority": "High",
        "difficulty": "medium",
        "readMins": 20,
        "codeStem": "NotificationSystem_demo",
        "altStems": ["LLD_questions"],
    },
    "NotificationSystem_demo": {"skip": True},
    "LoggingSystem_demo": {
        "slug": "logging-framework",
        "title": "Design Logging Framework",
        "category": "infra",
        "priority": "Medium",
        "difficulty": "easy",
        "readMins": 18,
    },
    "LRU_Cache_demo": {
        "slug": "lru-cache",
        "title": "Design LRU Cache",
        "category": "ds",
        "priority": "High",
        "difficulty": "medium",
        "readMins": 20,
    },
    "ParkingLot_demo": {
        "slug": "parking-lot",
        "title": "Design Parking Lot",
        "category": "management",
        "priority": "High",
        "difficulty": "medium",
        "readMins": 25,
    },
    "Payment_Service_Demo": {
        "slug": "payment-gateway",
        "title": "Design Payment Gateway",
        "category": "finance",
        "priority": "High",
        "difficulty": "hard",
        "readMins": 28,
    },
    "RateLimiter_demo": {
        "slug": "rate-limiter",
        "title": "Design Rate Limiter",
        "category": "infra",
        "priority": "High",
        "difficulty": "medium",
        "readMins": 22,
    },
    "Splitwise_LLD": {
        "slug": "splitwise",
        "title": "Design Splitwise",
        "category": "finance",
        "priority": "High",
        "difficulty": "hard",
        "readMins": 30,
    },
    "Splitwise_demo": {"skip": True},
    "TaskSchedulerDemo": {
        "slug": "task-scheduler",
        "title": "Design Task Scheduler",
        "category": "infra",
        "priority": "Medium",
        "difficulty": "medium",
        "readMins": 22,
    },
    "Task_Schedular_demo": {"skip": True},
    "TradingSystem_LLD": {
        "slug": "stock-exchange",
        "title": "Design Online Stock Exchange",
        "category": "finance",
        "priority": "High",
        "difficulty": "hard",
        "readMins": 35,
    },
}

CATEGORIES = [
    {"id": "games", "label": "Games & Puzzles"},
    {"id": "ds", "label": "Data Structures & Search"},
    {"id": "states", "label": "Managing States"},
    {"id": "management", "label": "Management Systems"},
    {"id": "messaging", "label": "Communication & Messaging"},
    {"id": "finance", "label": "Financial & Payment Systems"},
    {"id": "ecommerce", "label": "E-commerce & Booking"},
    {"id": "infra", "label": "Developer Tools & Infrastructure"},
]

CLARIFYING = {
    "parking-lot": [
        ("Should the lot support multiple floors?", "Yes — model floors, each with many spots."),
        ("Different vehicle sizes?", "Yes — bike, car, truck map to spot types."),
        ("Payment at exit?", "Yes — pluggable payment / pricing strategies."),
        ("Multiple entry/exit gates?", "Yes — gates allocate and release through a shared manager."),
    ],
    "chess-game": [
        ("Full FIDE rules including castling?", "Core piece moves + check/mate/stalemate for now; castling/en passant out of scope."),
        ("Support undo?", "Yes — keep a move history that can reverse captures/promotions."),
        ("AI opponent?", "Not for this interview — focus on board rules and game orchestration."),
    ],
    "elevator-system": [
        ("One elevator or a fleet?", "A fleet — assignment strategy picks which cabin handles a request."),
        ("Hall calls vs cabin calls?", "Both, via one Request abstraction."),
        ("Concurrency?", "Each elevator runs on its own thread with per-cabin locks."),
    ],
    "lru-cache": [
        ("Required complexity?", "get and put must be O(1)."),
        ("Eviction policy?", "Least recently used when capacity is exceeded."),
        ("Thread safety?", "Discuss; implementation may use synchronization if needed."),
    ],
    "notification-system": [
        ("Which channels?", "SMS, Email, Slack, WhatsApp — and easy to add more."),
        ("Retries on failure?", "Bounded retries, then mark FAILED."),
        ("Who owns delivery vs content?", "Service creates the notification; dispatcher + channels deliver."),
    ],
}


def extract_header(text: str) -> str:
    # Prefer a block comment that mentions requirements / LLD / FR
    for m in re.finditer(r"/\*(.*?)\*/", text, re.S):
        body = m.group(1).strip()
        if re.search(
            r"(LLD:|Functional|Non-Functional|Core Entities|\bFR:|Design notes)",
            body,
            re.I,
        ):
            return body
    m = re.search(r'"""(.*?)"""', text, re.S)
    if m:
        return m.group(1).strip()
    m = re.search(r"'''(.*?)'''", text, re.S)
    if m:
        return m.group(1).strip()
    lines = []
    for line in text.splitlines()[:120]:
        s = line.strip()
        if s.startswith("//") or s.startswith("#") or s.startswith("*"):
            lines.append(re.sub(r"^[\s/#*]+", "", line))
        elif lines and s == "":
            lines.append("")
        elif lines and not s.startswith("import") and not s.startswith("package"):
            break
    return "\n".join(lines).strip()


def parse_list_after(header: str, start_pat: str, end_pats: list[str]) -> list[str]:
    m = re.search(start_pat, header, re.S | re.I)
    if not m:
        return []
    rest = header[m.end() :]
    end = len(rest)
    for ep in end_pats:
        em = re.search(ep, rest, re.I | re.M)
        if em:
            end = min(end, em.start())
    block = rest[:end]
    items: list[str] = []
    for line in block.splitlines():
        raw = line.rstrip()
        t = raw.strip()
        if not t or set(t) <= set("-=*"):
            continue
        if re.match(r"(?i)^(design|core entit|out of scope|non-functional|functional)\b", t):
            break
        # continuation line (indented, no number/bullet)
        is_new = bool(re.match(r"^(\d+\.|[-*])\s+", t)) or (
            raw.startswith("    ") and not items
        )
        # FR: lines that are numbered OR start with capital after indent
        numbered = re.match(r"^(\d+\.|[-*])\s+(.*)$", t)
        if numbered:
            items.append(numbered.group(2).strip())
            continue
        # TaskScheduler style: bare lines under FR:
        if items and (raw.startswith("    ") or raw.startswith("\t")) and not re.match(
            r"^[A-Z]", t
        ):
            # likely continuation of previous wrapped sentence
            if items[-1].endswith((".", "?", ")", ",")) is False:
                items[-1] = items[-1] + " " + t
            else:
                items.append(t)
            continue
        if t and not t.lower().startswith("----"):
            # join soft-wrapped previous item
            if items and not items[-1].endswith((".", "?", ")")) and t[0].islower():
                items[-1] = items[-1] + " " + t
            else:
                items.append(t)
    return items


def parse_sections(header: str):
    # Also support "FR:" / "Non-Functional" shorthand headers
    header_norm = header
    header_norm = re.sub(r"(?m)^\s*FR:\s*$", "Functional Requirements:", header_norm)
    header_norm = re.sub(
        r"(?m)^\s*Non-Functional\s*$", "Non-Functional Requirements:", header_norm
    )

    fr = parse_list_after(
        header_norm,
        r"Functional Requirements:?\s*",
        [
            r"(?m)^Non-Functional",
            r"(?m)^Core Entities",
            r"(?m)^Design",
            r"(?m)^Design notes",
            r"Out of scope",
            r"(?m)^-{3,}",
        ],
    )
    nfr = parse_list_after(
        header_norm,
        r"Non-Functional Requirements:?\s*",
        [
            r"(?m)^Core Entities",
            r"(?m)^Design",
            r"(?m)^Design notes",
            r"(?m)^Functional",
            r"Out of scope",
            r"(?m)^-{3,}",
        ],
    )
    ents = parse_list_after(
        header_norm,
        r"Core Entities(?:\s*\([^)]*\))?:?\s*",
        [r"Out of scope", r"(?m)^Design", r"={5,}", r"(?m)^-{3,}"],
    )
    design = ""
    dm = re.search(
        r"(?ms)(?:Design notes:|Design(?:\s*\([^)]*\))?:)\s*(.*?)(?:Core Entities|Out of scope|={5,}|\Z)",
        header_norm,
    )
    if dm:
        design = re.sub(r"\s+", " ", dm.group(1)).strip(" -=")

    stop_words = ("requirement", "design:", "----")
    fr = [x for x in fr if not any(s in x.lower() for s in stop_words) and len(x) > 3][:12]
    nfr = [x for x in nfr if not any(s in x.lower() for s in stop_words) and len(x) > 3][:8]
    ents = [x for x in ents if len(x) > 2 and "out of scope" not in x.lower()][:16]
    return fr, nfr, ents, design[:1400]


def find_code_file(stem: str, meta: dict) -> dict:
    code_stem = meta.get("codeStem", stem)
    candidates = [code_stem] + meta.get("altStems", []) + [stem]
    if "notification" in meta.get("slug", ""):
        candidates = ["NotificationSystem_demo", "LLD_questions", stem]
    result = {}
    for lang, (rel, ext) in LANGS.items():
        found = None
        for c in candidates:
            fp = ROOT / rel / f"{c}{ext}"
            if fp.exists():
                # Paths are relative to lld-learn/ so the static site can fetch them.
                found = f"../{rel}/{c}{ext}".replace("\\", "/")
                break
        result[lang] = found
    return result


def main():
    java_dir = ROOT / "Java" / "Interview_problems"
    problems = []
    seen = set()

    for path in sorted(java_dir.glob("*.java")):
        stem = path.stem
        meta = META.get(
            stem,
            {
                "slug": stem.lower().replace("_", "-"),
                "title": "Design " + stem.replace("_", " "),
                "category": "management",
                "priority": "Medium",
                "difficulty": "medium",
                "readMins": 20,
            },
        )
        if meta.get("skip"):
            continue
        if meta["slug"] in seen:
            continue
        seen.add(meta["slug"])

        text = path.read_text(encoding="utf-8", errors="replace")
        header = extract_header(text)
        fr, nfr, ents, design = parse_sections(header)
        classes = re.findall(
            r"^(?:public\s+)?(?:abstract\s+)?(?:final\s+)?(?:class|interface|enum)\s+(\w+)",
            text,
            re.M,
        )
        classes = [
            c
            for c in classes
            if c != stem and not c.endswith("_demo") and "Demo" not in c
        ]

        intro = ""
        for line in header.splitlines():
            t = line.strip().strip("=")
            if (
                not t
                or t.startswith("LLD:")
                or "Requirement" in t
                or t.startswith("Interview")
                or t.startswith("Core")
                or t.startswith("Design")
                or set(t) <= set("-")
            ):
                continue
            if re.match(r"^\d+\.", t):
                break
            intro = t
            break

        problem = {
            "slug": meta["slug"],
            "title": meta["title"],
            "category": meta["category"],
            "priority": meta["priority"],
            "difficulty": meta["difficulty"],
            "readMins": meta["readMins"],
            "intro": intro
            or f"A low-level design walkthrough for {meta['title'].replace('Design ', '')}.",
            "clarifying": CLARIFYING.get(meta["slug"], []),
            "functional": fr,
            "nonFunctional": nfr,
            "entities": ents,
            "designNotes": design,
            "classes": classes[:25],
            "codeFiles": find_code_file(stem, meta),
            "sourceStem": stem,
        }
        problems.append(problem)

    # Fallbacks for thin headers
    for pr in problems:
        if pr["slug"] == "notification-system":
            pr["intro"] = (
                "Design a multi-channel notification system with retries, "
                "explicit delivery states, and Open/Closed channel extension."
            )
            pr["functional"] = [
                "Send a notification through SMS, EMAIL, SLACK, or WHATSAPP",
                "Add new channels without editing existing channel code",
                "Track delivery states: PENDING → SENT, or PENDING → FAILED",
                "Retry transient channel failures a bounded number of times",
            ]
            pr["nonFunctional"] = [
                "Extensibility: new channel = new class, not an if/else edit",
                "Reliability: bounded retries (not infinite, not zero)",
                "Decoupling: service owns what/to-whom; channels own how",
            ]
            pr["entities"] = [
                "NotificationType — SMS, EMAIL, SLACK, WHATSAPP",
                "NotificationStatus — PENDING, SENT, FAILED",
                "Notification — id, type, recipient, message, status, attempts",
                "NotificationChannel — strategy interface send(Notification)",
                "NotificationDispatcher — registry + retry loop",
                "NotificationService — facade that creates and dispatches",
            ]
            pr["designNotes"] = (
                "Strategy + registry: dispatcher maps NotificationType to a "
                "NotificationChannel. Slack is mocked to fail N times so the "
                "retry path is deterministic. NotificationService stays free "
                "of channel details."
            )
            pr["clarifying"] = [
                ("Which channels in v1?", "SMS, Email, Slack, WhatsApp — and easy to add more."),
                ("Retries on failure?", "Bounded retries, then mark FAILED."),
                ("Who owns delivery vs content?", "Service creates; dispatcher + channels deliver."),
            ]
        if pr["slug"] == "task-scheduler":
            pr["intro"] = (
                "Design a thread-safe task scheduler that runs work at a future time, "
                "supports priorities, cancellation, and recurring jobs."
            )
            pr["functional"] = [
                "Schedule a task for a future time",
                "Execute the task at the scheduled time",
                "Support task priority",
                "Support recurring tasks",
                "Cancel a scheduled task",
                "Execute tasks asynchronously",
            ]
            pr["nonFunctional"] = [
                "Thread-safe scheduling",
                "Dispatcher must not block on task work",
            ]
            pr["entities"] = [
                "Task",
                "ScheduledTask (Delayed)",
                "Priority",
                "TaskScheduler (DelayQueue + worker pool)",
            ]
            pr["designNotes"] = (
                "ScheduledTask implements Delayed for DelayQueue. A single dispatcher "
                "thread blocks on queue.take() and hands execution to a worker pool. "
                "Cancellation is a soft-delete flag; recurring tasks are re-inserted "
                "after each run."
            )
            pr["clarifying"] = [
                ("Recurring jobs?", "Yes — re-enqueue with a new execution time after each run."),
                ("Who runs the work?", "A worker ExecutorService; the dispatcher only schedules."),
            ]
            pr["classes"] = [
                "Task",
                "EmailTask",
                "PaymentTask",
                "Priority",
                "ScheduledTask",
                "TaskScheduler",
            ]
        if pr["slug"] == "parking-lot":
            pr["nonFunctional"] = pr["nonFunctional"] or [
                "Extensible spot/vehicle types and payment strategies",
                "Clear separation between allocation, occupancy state, and billing",
            ]
            pr["clarifying"] = pr["clarifying"] or [
                ("Should the lot support multiple floors?", "Yes — model floors, each with many spots."),
                ("Different vehicle sizes?", "Yes — bike, car, truck map to spot types."),
                ("Payment at exit?", "Yes — pluggable payment / pricing strategies."),
            ]
            pr["designNotes"] = pr["designNotes"] or (
                "State pattern for spot occupancy (NoVehicleState / HasVehicleState). "
                "Singleton ParkingLotManager coordinates floors and gates. "
                "Payment and pricing are strategy interfaces."
            )
        if pr["slug"] == "file-system":
            pr["intro"] = (
                "Design an in-memory hierarchical file system with files, "
                "directories, and path-based operations."
            )
            pr["functional"] = pr["functional"] or [
                "Create files and directories under a path",
                "List directory contents",
                "Read and write file content",
                "Delete nodes from the tree",
                "Navigate with absolute paths",
            ]
            pr["nonFunctional"] = pr["nonFunctional"] or [
                "Clear composite tree model for files and directories",
                "Path resolution centralized in the manager facade",
            ]
            pr["entities"] = pr["entities"] or [
                "FileSystemNode (base)",
                "File",
                "Directory",
                "FileSystemManager",
            ]
            pr["designNotes"] = pr["designNotes"] or (
                "Composite-style tree: Directory holds children FileSystemNodes; "
                "File holds content. FileSystemManager is the facade for path resolution."
            )
            pr["clarifying"] = pr["clarifying"] or [
                ("Only in-memory?", "Yes — no real disk I/O for this interview."),
                ("Permissions / users?", "Out of scope; focus on the tree model."),
            ]
        if pr["slug"] == "kafka":
            pr["intro"] = (
                "Design a simplified Kafka-like pub-sub system with topics, "
                "partitions, producers, and consumer groups."
            )
            pr["functional"] = pr["functional"] or [
                "Create topics with multiple partitions",
                "Produce messages with a partition key",
                "Consume messages as part of a consumer group",
                "Track offsets per partition",
                "Support pluggable partition selection",
            ]
            pr["entities"] = pr["entities"] or [
                "Message",
                "Topic",
                "Partition",
                "Broker",
                "Producer",
                "Consumer",
                "ConsumerGroup",
                "OffsetManager",
                "PartitionSelectionStrategy",
            ]
            pr["designNotes"] = pr["designNotes"] or (
                "Strategy pattern for partition selection (hash / round-robin). "
                "Broker owns topics; consumer groups share partition ownership via offsets."
            )

    payload = {
        "brand": "Weekday LLD",
        "tagline": "Low-Level Design Interviews",
        "categories": CATEGORIES,
        "problems": problems,
        "languages": [
            {"id": "java", "label": "Java", "ext": ".java"},
            {"id": "javascript", "label": "JavaScript", "ext": ".js"},
            {"id": "python", "label": "Python", "ext": ".py"},
            {"id": "cpp", "label": "C++", "ext": ".cpp"},
            {"id": "golang", "label": "Go", "ext": ".go"},
        ],
        "compare": {
            "algomaster": [
                "Lesson-first walkthrough (clarify → entities → class tables → code → extensions → quiz)",
                "Interactive diagrams, in-browser editor, audio, progress, certificates",
                "One language focus with premium gating",
            ],
            "yours": [
                "Working multi-language demos (Java, JS, Python, C++, Go) for the same problem",
                "Design notes live in source file headers (FR / NFR / entities / patterns)",
                "Code-complete interview solutions you can run locally",
            ],
            "siteGoal": [
                "Present your existing Interview_problems in an AlgoMaster-style reading layout",
                "Add clarifying Q&A, entity overview, and language-switchable source view",
                "Keep it a simple static HTML site — no backend required",
            ],
        },
    }

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "problems.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    (OUT / "problems.js").write_text(
        "window.LLD_DATA = " + json.dumps(payload) + ";\n", encoding="utf-8"
    )
    print(f"Wrote {len(problems)} problems -> {OUT}")
    for pr in problems:
        print(
            f"  {pr['slug']:28} FR={len(pr['functional']):2} "
            f"NFR={len(pr['nonFunctional']):2} ENT={len(pr['entities']):2}"
        )


if __name__ == "__main__":
    main()
