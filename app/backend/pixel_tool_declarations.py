"""Shared OpenAPI-style function declarations for Pixel (REST + Live)."""

FACE_ANIMATION_FUNCTION_DECLARATION = {
    "name": "face_animation",
    "description": (
        "Animates Pixel's face on the round display for conversational or emotional states only. "
        "Pass only which face to show: speaking, happy, or mad."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "animation": {
                "type": "string",
                "enum": ["speaking", "happy", "mad"],
                "description": "Which face animation to display: speaking, happy, or mad.",
            },
        },
        "required": ["animation"],
    },
}
