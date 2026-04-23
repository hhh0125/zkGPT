# -*- coding: utf-8 -*-
from typing import List
import torch
from .transcript import Transcript
from ..serialize import serialize, from_le_bytes_mod_order

class IOPTranscript:

    def __init__(self, label: bytes):
        self.transcript = Transcript(label)
        self.is_empty = True

    def append_message(self, label: bytes, msg: bytes):
        self.transcript.append_message(label, msg)
        self.is_empty = False

    def append_field_element(self, label: bytes, field_elem):
        buf = serialize(field_elem)
        self.append_message(label, buf)

    def append_serializable_element(self, label: bytes, elem):
        buf = serialize(elem)
        self.append_message(label, buf)

    def get_and_append_challenge(self, label: bytes) -> torch.Tensor:
        '''
        Generate a list of challenges from the current transcript
        and append them to the transcript.
        The output field element are statistical uniform as long
        as the field has a size less than 2^384.
        '''
        buf = bytearray(64)
        modified_buf = self.transcript.challenge_bytes(label, buf)
        challenge = from_le_bytes_mod_order(modified_buf)
        self.append_serializable_element(label, challenge)
        return challenge

    def get_and_append_challenge_vectors(self, label: bytes, length: int) -> List[torch.Tensor]:
        res = []
        for _ in range(length):
            res.append(self.get_and_append_challenge(label))
        return res
