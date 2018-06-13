#!/usr/bin/env python
# -*- coding:utf-8 -*-

import sys
import unittest

sys.path.insert(0, '../build/lib.linux-x86_64-2.7')

import seeq

class TestSeeq(unittest.TestCase):

   def test_matchPrefix(self):
      matcher = seeq.compile("CGCTAATTAATGGAAT", 3)

      nomatch = "ATGCTGATGCTGGGGG"
      match = "GGGGCGCTAATAATGGAATGGGG"

      self.assertEqual(matcher.matchPrefix(nomatch, True), None)
      self.assertEqual(matcher.matchPrefix(nomatch, False), None)
      self.assertEqual(matcher.matchPrefix(match, True),
            "GGGGCGCTAATAATGGAAT")
      self.assertEqual(matcher.matchPrefix(match, False), "GGGG")

   def test_matchSuffix(self):
      matcher = seeq.compile("CGCTAATTAATGGAAT", 3)

      nomatch = "ATGCTGATGCTGGGGG"
      match = "GGGGCGCTAATAATGGAATGGGG"

      self.assertEqual(matcher.matchSuffix(nomatch, True), None)
      self.assertEqual(matcher.matchSuffix(nomatch, False), None)
      self.assertEqual(matcher.matchSuffix(match, True),
            "CGCTAATAATGGAATGGGG")
      self.assertEqual(matcher.matchSuffix(match, False), "GGGG")

if __name__ == '__main__':
   assert seeq.__file__ == '../build/lib.linux-x86_64-2.7/seeq.so'
   unittest.main()
