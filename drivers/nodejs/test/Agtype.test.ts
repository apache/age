/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

import { AGTypeParse } from '../src'

describe('Parsing', () => {
  it('Vertex', () => {
    expect(
      AGTypeParse('{"id": 844424930131969, "label": "Part", "properties": {"part_num": "123", "number": 3141592653589793, "float": 3.141592653589793}}::vertex')
    ).toStrictEqual(new Map<string, any>(Object.entries({
      id: 844424930131969,
      label: 'Part',
      properties: new Map(Object.entries({
        part_num: '123',
        number: 3141592653589793,
        float: 3.141592653589793
      }))
    })))
  })

  it('Edge', () => {
    expect(
      AGTypeParse('{"id": 1125899906842625, "label": "used_by", "end_id": 844424930131970, "start_id": 844424930131969, "properties": {"quantity": 1}}::edge')
    ).toStrictEqual(new Map(Object.entries({
      id: 1125899906842625,
      label: 'used_by',
      end_id: 844424930131970,
      start_id: 844424930131969,
      properties: new Map(Object.entries({ quantity: 1 }))
    })))
  })

  it('Path', () => {
    expect(
      AGTypeParse('[{"id": 844424930131969, "label": "Part", "properties": {"part_num": "123"}}::vertex, {"id": 1125899906842625, "label": "used_by", "end_id": 844424930131970, "start_id": 844424930131969, "properties": {"quantity": 1}}::edge, {"id": 844424930131970, "label": "Part", "properties": {"part_num": "345"}}::vertex]::path')
    ).toStrictEqual([
      new Map(Object.entries({
        id: 844424930131969,
        label: 'Part',
        properties: new Map(Object.entries({ part_num: '123' }))
      })),
      new Map(Object.entries({
        id: 1125899906842625,
        label: 'used_by',
        end_id: 844424930131970,
        start_id: 844424930131969,
        properties: new Map(Object.entries({ quantity: 1 }))
      })),
      new Map(Object.entries({
        id: 844424930131970,
        label: 'Part',
        properties: new Map(Object.entries({ part_num: '345' }))
      }))
    ])
  })

  it('Null Properties', () => {
    expect(
      AGTypeParse('{"id": 1688849860263937, "label": "car", "properties": {}}::vertex')
    ).toStrictEqual(new Map<string, any>(Object.entries({
      id: 1688849860263937,
      label: 'car',
      properties: new Map(Object.entries({}))
    })))
  })

  it('Nested Agtype', () => {
    expect(
      AGTypeParse('{"id": 1688849860263937, "label": "car", "properties": {"a": {"b":{"c":{"d":[1, 2, "A"]}}}}}::vertex')
    ).toStrictEqual(new Map<string, any>(Object.entries({
      id: 1688849860263937,
      label: 'car',
      properties: new Map<string, any>(Object.entries({
        a: new Map<string, any>(Object.entries({
          b: new Map<string, any>(Object.entries({
            c: new Map<string, any>(Object.entries({
              d: [
                1,
                2,
                'A'
              ]
            }))
          }))
        }))
      }))
    })))
  })

  it('Large integer uses BigInt when exceeding MAX_SAFE_INTEGER', () => {
    // 2^53 = 9007199254740992 exceeds MAX_SAFE_INTEGER (2^53 - 1)
    const result = AGTypeParse('{"id": 9007199254740993, "label": "test", "properties": {}}::vertex')
    const id = (result as Map<string, any>).get('id')
    expect(typeof id).toBe('bigint')
    expect(id).toBe(BigInt('9007199254740993'))
  })

  it('Safe integers remain as Number type', () => {
    const result = AGTypeParse('{"id": 844424930131969, "label": "test", "properties": {}}::vertex')
    const id = (result as Map<string, any>).get('id')
    expect(typeof id).toBe('number')
    expect(id).toBe(844424930131969)
  })

  it('Small integers remain as Number type', () => {
    const result = AGTypeParse('42')
    expect(typeof result).toBe('number')
    expect(result).toBe(42)
  })

  it('Negative integers parsed correctly', () => {
    const result = AGTypeParse('-100')
    expect(typeof result).toBe('number')
    expect(result).toBe(-100)
  })
})
