# Протокол инвокера

Инвокер принимает задания от Message Broker'а по протоколу AMQP.
Задание &mdash; это JSON-закодированный объект.

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
    "processes": {
        "1": {
            "language": "gxx",
            "executable": "5030ba6f9f3787e13df3c821acef52cca95dbbc9af948a27d75ba5a3a6b31c80",
            "arguments": {
                "source": "checker.cpp",
                "flags": ["-std=c++17", "-Wall", "-O2", "-lm"]
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
        "input": "d9f5be097c6452cf2f0f5a8f4921952fc6c93c05550f1c613aa8c458f321d4cb"
    },
    "pipes": [
        {
            "read": {
                "resource": "input"
            },
            "write": {
                "process": "1",
                "fd": 0
            }
        },
        {
            "read": {
                "process": "1",
                "fd": 1
            },
            "write": {
                "artifact": "output"
            }
        }
    ],
    "artifacts": ["output"],
    "id": "f25490ff-85ba-4e28-b175-7551f3e81f92"
}
```

Пример ответа на задание запуска:

```json
{
    "type": "invoke_response",
    "id": "f25490ff-85ba-4e28-b175-7551f3e81f92",
    "artifacts": {
        "output": "1f2680d4c2b9783129d1da8dcfff2ad0bfb96f1e79337590c9ce7abe54847b04"
    },
    "processes": {
        "1": {
            "cpu": 47,
            "real": 89,
            "memory": 4910,
            "exitcode": 0,
            "flags": 0
        }
    }
}
```
