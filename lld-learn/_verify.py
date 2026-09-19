import urllib.request

for url in [
    "http://127.0.0.1:8877/lld-learn/",
    "http://127.0.0.1:8877/lld-learn/app.js",
    "http://127.0.0.1:8877/lld-learn/data/problems.js",
    "http://127.0.0.1:8877/Java/Interview_problems/ParkingLot_demo.java",
]:
    try:
        r = urllib.request.urlopen(url, timeout=5)
        print(url, r.status, "len", len(r.read()))
    except Exception as e:
        print(url, "FAIL", e)
