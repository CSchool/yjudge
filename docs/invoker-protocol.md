# Протокол инвокера

Инвокер принимает задания от Message Broker'а по протоколу AMQP.
Задание &mdash; это Msgpack-закодированный объект.

Пример задания компиляции (запрос к инвокеру):

```json
{
    "type": "compile_request",
    "language": "gxx",
    "sources": {
        "checker.cpp": "e05b5d8142c80082c30358c5f0760617c5cac46a102d09849d71007ffa371072",
        "testlib.h": "473d7511e3e186a767f7bce6134fe0723b01d765e8a9f42d6b9af4b96b165041"
    },
    "arguments": {
        "source": "checker.cpp",
        "flags": ["-std=c++17", "-Wall", "-O2", "-lm"]
    },
    "id": "c6d30981-d943-4a0f-91a4-f74c11b0357b"
}
```

Пример ответа на задание компиляции:

```json
{
    "type": "compile_response",
    "id": "c6d30981-d943-4a0f-91a4-f74c11b0357b",
    "success": true,
    "artifacts": {
        "executable": "5030ba6f9f3787e13df3c821acef52cca95dbbc9af948a27d75ba5a3a6b31c80",
        "log": "f074902f1b38e6b620e2ab53a01cef76bf64fdfe47823b264c528c348aebb278"
    }
}
```

Пример задания запуска (запрос к инвокеру):

```json
{
    "type": "invoke_request",
    "pipeline": [
        {
            "processes": {
                "solution": {
                    "language": "python3",
                    "executable": "522e65ba5a17fefbbedccc1b650ecedde44898716446f7113a235a634af6e78f",
                    "arguments": {
                        "source": "solution.py"
                    },
                    "limits": {
                        "cpu": 1000,
                        "real": 5000,
                        "memory": 262144,
                        "fsize": 16777216
                    },
                    "cmdline": []
                }
            },
            "resources": {
                "input": {
                    "hash": "d9f5be097c6452cf2f0f5a8f4921952fc6c93c05550f1c613aa8c458f321d4cb"
                }
            },
            "pipes": [
                {
                    "read": {
                        "resource": "input"
                    },
                    "write": {
                        "process": "solution",
                        "fd": 0
                    }
                },
                {
                    "read": {
                        "process": "solution",
                        "fd": 1
                    },
                    "write": {
                        "artifact": "output"
                    }
                }
            ],
            "stop": "on_failure",
            "artifacts": {
                "output": {
                    "private": true
                }
            }
        },
        {
            "processes": {
                "checker": {
                    "language": "gxx",
                    "executable": "5030ba6f9f3787e13df3c821acef52cca95dbbc9af948a27d75ba5a3a6b31c80",
                    "arguments": {
                        "source": "checker.cpp",
                        "flags": ["-std=c++17", "-Wall", "-O2", "-lm"]
                    },
                    "limits": {
                        "real": 10000,
                    },
                    "cmdline": ["input.txt", "output.txt", "answer.txt"]
                }
            },
            "pipes": [
                {
                    "read": {
                        "process": "checker",
                        "fd": 1
                    },
                    "write": {
                        "artifact": "log"
                    }
                }
            ],
            "resources": {
                "input": {
                    "hash": "d9f5be097c6452cf2f0f5a8f4921952fc6c93c05550f1c613aa8c458f321d4cb",
                    "filename": "input.txt"
                },
                "output": {
                    "intermediate": true,
                    "filename": "output.txt"
                },
                "answer": {
                    "hash": "2e4acd714d8446e919fbdd69a5784d80f26dbc00b584d478115b2294ce320b40",
                    "filename": "answer.txt"
                }
            },
            "stop": "always",
            "artifacts": {
                "log": {
                    "private": false
                }
            }
        }
    ],
    "id": "f25490ff-85ba-4e28-b175-7551f3e81f92"
}
```

Пример ответа на задание запуска:

```json
{
    "type": "invoke_response",
    "id": "f25490ff-85ba-4e28-b175-7551f3e81f92",
    "pipeline": [
        {
            "processes": {
                "solution": {
                    "cpu": 123,
                    "real": 217,
                    "memory": 14926,
                    "exitcode": 0,
                    "flags": 0
                }
            }
        },
        {
            "processes": {
                "checker": {
                    "cpu": 47,
                    "real": 89,
                    "memory": 4910,
                    "exitcode": 0,
                    "flags": 0
                }
            }
        }
    ],
    "artifacts": {
        "output": "229a6378bd0a924122cc01571318d4985545997652104164a9e1440234ab0445",
        "log": "1f2680d4c2b9783129d1da8dcfff2ad0bfb96f1e79337590c9ce7abe54847b04"
    }
}
```

Пример запроса файла (запрос от инвокера):

```json
{
    "type": "file_request",
    "hash": "e05b5d8142c80082c30358c5f0760617c5cac46a102d09849d71007ffa371072"
}
```

Пример ответа на запрос файла:

```json
{
    "type": "file_response",
    "hash": "e05b5d8142c80082c30358c5f0760617c5cac46a102d09849d71007ffa371072",
    "mime": "application/gzip",
    "content": "<RAW BINARY DATA HERE>"
}
```

Пример запроса на выгрузку артифакта (запрос от инвокера)

```json
{
    "type": "artifact_upload",
    "hash": "e05b5d8142c80082c30358c5f0760617c5cac46a102d09849d71007ffa371072",
    "mime": "application/gzip",
    "content": "<RAW BINARY DATA HERE>"
}
```
