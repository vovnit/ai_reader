import json
import re
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Dict, List

# Answers from `mock_answer` instead of a model, so a routine run costs nothing.
MOCK_ENDPOINT = "mock://ai"
ATTEMPTS = 5
# Seconds before the first retry; each later one waits twice as long.
RETRY_DELAY = 2.0


class EndpointError(Exception):
    pass


@dataclass(frozen=True)
class Completion:
    content: str
    input_tokens: int
    output_tokens: int


def complete(endpoint: str, model: str, token: str, messages: List[Dict[str, str]]) -> Completion:
    """Asks an OpenAI-compatible endpoint for a JSON answer, as the apps do."""
    if endpoint.strip() == MOCK_ENDPOINT:
        return Completion(mock_answer(messages), 0, 0)

    url = endpoint.strip().rstrip("/") + "/chat/completions"
    body = {"model": model, "messages": messages, "response_format": {"type": "json_object"}}
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token

    attempt = 0
    while True:
        attempt += 1
        try:
            request = urllib.request.Request(url, data=json.dumps(body).encode("utf-8"), headers=headers)
            with urllib.request.urlopen(request, timeout=300) as response:
                answer = json.load(response)
            usage = answer.get("usage") or {}
            return Completion(
                answer["choices"][0]["message"]["content"] or "",
                usage.get("prompt_tokens", 0),
                usage.get("completion_tokens", 0),
            )
        except urllib.error.HTTPError as error:
            with error:
                detail = error.read().decode("utf-8", "replace")[:300]
            # A service without JSON mode says so; ask again without it.
            if error.code == 400 and "response_format" in detail and "response_format" in body:
                del body["response_format"]
                continue
            # Rate limits and server trouble pass; anything else will not.
            if error.code != 429 and error.code < 500 or attempt >= ATTEMPTS:
                raise EndpointError("{} answered {}: {}".format(url, error.code, detail))
        except (urllib.error.URLError, OSError) as error:
            if attempt >= ATTEMPTS:
                raise EndpointError("could not reach {}: {}".format(url, error))
        except (ValueError, KeyError, IndexError, TypeError) as error:
            raise EndpointError("{} sent an answer that is not a chat completion ({})".format(url, error))
        time.sleep(RETRY_DELAY * 2 ** (attempt - 1))


def mock_answer(messages: List[Dict[str, str]]) -> str:
    """Defines every numbered word in the question, as a model would, from
    nothing but the question itself."""
    question = messages[-1]["content"]
    words = [
        {"n": int(number), "lemma": spelling.lower(), "form_note": "", "meaning": "«{}» в книге (макет)".format(spelling)}
        for number, spelling in re.findall(r"^(\d+)\. (.+)$", question, re.MULTILINE)
    ]
    return json.dumps({"words": words}, ensure_ascii=False)
