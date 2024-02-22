#!/usr/bin/env python3
import urllib, urllib.request, subprocess, signal, sys, json, time

def run_tests(server):
    start_time = time.time()
    total_timeout_seconds = 60
    
    # Timeout if the process doesn't print out a line within some seconds
    timeout_seconds = 10
    def timeout(sig, frm):
        print("no new line after " + str(timeout_seconds) + " seconds")
        server.kill()
        sys.exit(1)

    signal.signal(signal.SIGALRM, timeout)
    signal.alarm(timeout_seconds)

    while True:
        line = server.stdout.readline()
        # Only continue when the evaluation is completed
        if "Evaluation completed" in line:
            signal.alarm(0)
            break
        if server.poll() != None:
            raise Exception("Server died")
        if time.time() - start_time > total_timeout_seconds:
            raise Exception("Failed to finish within " + str(total_timeout_seconds) + " seconds")
        # Only reset the timer if the line is nonempty
        if line != "":
            signal.alarm(timeout_seconds)

    print('Evaluation completed, testing requests')

    def request(query):
        with urllib.request.urlopen("http://127.0.0.1:9090/" + query) as httpresponse:
            return httpresponse.read().decode(httpresponse.headers.get_content_charset("utf-8"))

    target_blob = "1000000000000000000000000000000000000000000000000000000000000024"
    
    result = json.loads(request("explanations?handle=" + target_blob))

    assert result["target"] == target_blob, "Blob's explanations' target should be itself"
    relations = result["relations"]
    identity_relation = {"relation": "Eval", "lhs": target_blob, "rhs": target_blob }
    assert identity_relation in relations, "Blob should evaluate to itself"

    original_thunk = [r for r in relations if r["relation"] == "Eval" and r["rhs"] == target_blob][0]["lhs"]

    result = json.loads(request("relation?handle=" + original_thunk + "&op=1"))

    relation = result['relation']
    assert relation['lhs'] == original_thunk, "thunk's eval relation should have itself on the left hand side"
    assert relation['rhs'] == target_blob, "blob's explanation should eval to the blob"


server = subprocess.Popen(["src/tester/http-tester", "9090", "src/tester/http-client", "tree:4", "string:unused", "thunk:", "tree:3", "string:nothing", "ref:compile-encode", "file:testing/wasm-examples/add-simple.wasm", "uint32:9", "uint32:7"], stdout=subprocess.PIPE, encoding='utf-8')

try:
    run_tests(server)
    print('Tests succeded')
except Exception as e:
    server.kill()
    raise e

server.kill()

