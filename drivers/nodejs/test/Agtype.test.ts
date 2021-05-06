import {AGTypeParse} from "../dist";

describe("Parsing", () => {

    it("Vertex", async () => {
        expect(
            AGTypeParse('{"id": 844424930131969, "label": "Part", "properties": {"part_num": "123", "number": 31415926535897932384626433832795, "float": 3.1415926535897932384626433832795}}::vertex')
        ).toStrictEqual(new Map<string, any>(Object.entries({
            id: 844424930131969,
            label: 'Part',
            properties: new Map(Object.entries({
                part_num: '123',
                number: 31415926535897932384626433832795,
                float: 3.1415926535897932384626433832795
            }))
        })));
    });
    it("Edge", async () => {
        expect(
            AGTypeParse('{"id": 1125899906842625, "label": "used_by", "end_id": 844424930131970, "start_id": 844424930131969, "properties": {"quantity": 1}}::edge')
        ).toStrictEqual(new Map(Object.entries({
            id: 1125899906842625,
            label: 'used_by',
            end_id: 844424930131970,
            start_id: 844424930131969,
            properties: new Map(Object.entries({quantity: 1}))
        })));
    });
    it("Path", async () => {
        expect(
            AGTypeParse('[{"id": 844424930131969, "label": "Part", "properties": {"part_num": "123"}}::vertex, {"id": 1125899906842625, "label": "used_by", "end_id": 844424930131970, "start_id": 844424930131969, "properties": {"quantity": 1}}::edge, {"id": 844424930131970, "label": "Part", "properties": {"part_num": "345"}}::vertex]::path')
        ).toStrictEqual([
            new Map(Object.entries({
                'id': 844424930131969,
                'label': 'Part',
                'properties': new Map(Object.entries({'part_num': '123'}))
            })),
            new Map(Object.entries({
                'id': 1125899906842625,
                'label': 'used_by',
                'end_id': 844424930131970,
                'start_id': 844424930131969,
                'properties': new Map(Object.entries({'quantity': 1})),
            })),
            new Map(Object.entries({
                'id': 844424930131970,
                'label': 'Part',
                'properties': new Map(Object.entries({'part_num': '345'}))
            }))
        ]);
    });
});
