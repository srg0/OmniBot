"""Semantic router: classify user prompts into Maps vs Search retrieval.

This uses `semantic-router[fastembed]` with two routes:
- maps: navigation / local place lookup
- search: weather + general web questions

The route examples are intentionally "full prompts" instead of tiny keywords so the
embedding model sees realistic requests.
"""

from __future__ import annotations

import os
import re
from pathlib import Path
from typing import Literal

RetrievalMode = Literal["maps", "search"]
RouteReason = Literal[
    "empty_input",
    "weather_intent",
    "semantic_maps_route",
    "semantic_search_route",
    "semantic_router_error",
    "destination_keyword",
    "fallback_search",
]

_router = None


# If the user clearly wants weather info, always use Google Search (not Maps grounding).
_MAPS_NAV_HINTS = (
    "near me",
    "nearby",
    "close to me",
    "directions",
    "navigate",
    "route to",
    "driving directions",
    "how far",
    "how long to drive",
    "how long is the drive",
    "walk to",
    "drive to",
    "traffic",
    "commute",
    "miles from",
    "km from",
    "eta ",
)

_DESTINATION_DIRECTION_PATTERNS = (
    r"\bdirections?\s+to\b",
    r"\bdriving\s+directions?\b",
    r"\bnavigate\s+to\b",
    r"\broute\s+to\b",
    r"\btake\s+me\s+to\b",
    r"\bhow\s+do\s+i\s+get\s+to\b",
    r"\bhow\s+can\s+i\s+get\s+to\b",
    r"\bway\s+to\b",
    r"\b(?:distance|eta)\s+to\b",
    r"\bhow\s+far\s+(?:is\s+it\s+)?to\b",
    r"\bhow\s+long\s+(?:is\s+the\s+drive|to\s+get)\s+to\b",
)

_LOCAL_PLACE_HINTS = (
    "near me",
    "nearby",
    "closest",
    "nearest",
    "around me",
    "in this area",
)

_PLACE_CATEGORY_HINTS = (
    "publix",
    "grocery",
    "supermarket",
    "store",
    "restaurant",
    "coffee",
    "gas station",
    "pharmacy",
    "hospital",
    "bank",
    "atm",
    "hotel",
    "parking",
    "ev charging",
    "charger",
)

_MAPS_ROUTE_UTTERANCES = [
    "Give me directions to Orlando International Airport.",
    "Navigate to the nearest gas station.",
    "How do I get to downtown from here?",
    "Show driving directions to Disney Springs.",
    "Route me to the closest hospital.",
    "How far is it to Tampa from my location?",
    "How long is the drive to Miami right now?",
    "Take me to the nearest Publix.",
    "Find a coffee shop near me and navigate there.",
    "Directions to the closest pharmacy please.",
    "Where is the nearest EV charging station?",
    "Find parking near me.",
    "Show me nearby restaurants.",
    "Navigate to the bank closest to me.",
    "What's the quickest route to Jacksonville?",
    "Get me walking directions to the convention center.",
]

_SEARCH_ROUTE_UTTERANCES = [
    "What's the weather today in Orlando?",
    "Will it rain tomorrow in Tampa?",
    "Show the weekly forecast for Miami.",
    "What temperature is it right now?",
    "What is quantum computing?",
    "Who won the game last night?",
    "Search the web for latest AI news.",
    "Explain how electric cars work.",
    "What are the best places to visit in Florida?",
    "What's the current stock price of AAPL?",
    "How do I reset my Wi-Fi router?",
    "Summarize today's top headlines.",
    "What's the difference between HTTP and HTTPS?",
    "Tell me a quick history of NASA.",
    "How do I bake sourdough bread?",
    "What's the meaning of life?",
]


def _get_router():
    """Build and cache semantic router lazily."""
    global _router
    if _router is not None:
        return _router

    from semantic_router import Route
    from semantic_router.encoders import FastEmbedEncoder
    from semantic_router.routers import SemanticRouter

    try:
        # Keep model cache in-project so downloads happen once per workspace.
        cache_dir = Path(__file__).resolve().parent / "models" / "fastembed_cache"
        os.makedirs(cache_dir, exist_ok=True)
        encoder = FastEmbedEncoder(cache_dir=str(cache_dir))
    except TypeError:
        # Compatibility fallback for older encoder signatures.
        encoder = FastEmbedEncoder()

    maps_route = Route(name="maps", utterances=_MAPS_ROUTE_UTTERANCES)
    search_route = Route(name="search", utterances=_SEARCH_ROUTE_UTTERANCES)
    _router = SemanticRouter(
        encoder=encoder,
        routes=[maps_route, search_route],
        auto_sync="local",
    )
    return _router


def _looks_like_weather_intent(text: str) -> bool:
    """True when the turn is primarily about weather/conditions (Search), not POI/navigation (Maps)."""
    t = (text or "").strip().lower()
    if not t:
        return False
    if any(h in t for h in _MAPS_NAV_HINTS):
        return False

    # Any standalone "weather" -> Search for robust weather routing.
    if re.search(r"\bweather\b", t):
        return True

    if re.search(r"\bwhat'?s?\s+the\s+weather\b", t):
        return True
    if re.search(r"\bhow'?s?\s+the\s+weather\b", t):
        return True
    if re.search(r"\bweather\s+(like|in|for|at|near|around|today|tomorrow|now)\b", t):
        return True
    if re.search(r"\b(forecast|temperature|humidity|precipitation|dew\s*point|wind\s*chill|heat\s*index|uv\s*index)\b", t):
        return True
    if re.search(r"\bwill\s+it\s+(rain|snow)\b", t):
        return True
    if re.search(r"\bis\s+it\s+(going\s+to\s+)?(rain|snow|snowing|raining)\b", t):
        return True
    if re.search(r"\bchance\s+of\s+(rain|snow|storms?)\b", t):
        return True
    if re.search(r"\bhow\s+(hot|cold|warm|cool)\b", t):
        return True
    if re.search(r"\bis\s+it\s+(cold|hot|warm|cool)\b", t):
        return True
    if re.search(r"\bwhat\s+to\s+wear\b", t) and re.search(
        r"\b(rain|snow|cold|hot|warm|coat|jacket|umbrella|weather)\b", t
    ):
        return True

    weather_tokens = (
        "current conditions",
        "air quality",
        "pollen count",
        "sunrise",
        "sunset",
        "accuweather",
        "weather channel",
        "meteorologist",
        "weather report",
        "local weather",
        "today's high",
        "todays high",
        "tonight's low",
        "tonights low",
    )
    if any(tok in t for tok in weather_tokens):
        return True

    return False


def _looks_like_destination_or_directions_intent(text: str) -> bool:
    t = (text or "").strip().lower()
    if not t:
        return False
    if any(re.search(p, t) for p in _DESTINATION_DIRECTION_PATTERNS):
        return True
    has_local_hint = any(h in t for h in _LOCAL_PLACE_HINTS)
    has_place_hint = any(p in t for p in _PLACE_CATEGORY_HINTS)
    return has_local_hint and has_place_hint


def classify_retrieval_with_reason(text: str) -> tuple[RetrievalMode, RouteReason]:
    """Return retrieval mode and why that route was selected."""
    stripped = (text or "").strip()
    if not stripped:
        return "search", "empty_input"
    if _looks_like_weather_intent(stripped):
        return "search", "weather_intent"

    try:
        router = _get_router()
        choice = router(stripped)
        route_name = getattr(choice, "name", None) if choice is not None else None
        if route_name == "maps":
            return "maps", "semantic_maps_route"
        if route_name == "search":
            return "search", "semantic_search_route"
    except Exception:
        # Keep chat resilient: fallback to deterministic keyword behavior.
        if _looks_like_destination_or_directions_intent(stripped):
            return "maps", "destination_keyword"
        return "search", "semantic_router_error"

    if _looks_like_destination_or_directions_intent(stripped):
        return "maps", "destination_keyword"
    return "search", "fallback_search"


def classify_retrieval(text: str) -> RetrievalMode:
    """Return ``maps`` for destination/directions turns; otherwise ``search``."""
    mode, _ = classify_retrieval_with_reason(text)
    return mode
